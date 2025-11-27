#!/usr/bin/env python3

import argparse
from xml.etree import ElementTree
from sys import stdin, stdout

## Simple C generator

class CGen:
   def __init__(self, file, columns = 3, /):
      self.file = file
      self.tab = " " * columns
      self.count = 0
      self.content = False
   def newline(self):
      if self.content:
         if self.count > 0:
            self.file.write(",\n")
         else:
            self.file.write(";\n\n")
         self.content = False
      else:
         self.file.write("\n")
   def prep(self, line):
      if self.content:
         self.newline()
      self.file.write(f"#{line}\n")
   def line(self, line):
      self.newline()
      self.file.write(self.tab * self.count)
      self.file.write(line)
      self.content = not not line
   def open(self, line = "", c = 1, /):
      self.newline()
      self.file.write(self.tab * self.count)
      if line:
         self.file.write(f"{line} = ")
      self.file.write("{" * c)
      self.count += c
      self.content = False
   def close(self, c = 1, /):
      self.count -= c
      self.file.write(f"\n{self.tab * self.count}{'}' * c}")
      self.content = True
   def comment(self, lines):
      for line in [l.rstrip('\\') for l in lines.strip().split("\n")]:
         self.newline()
         self.file.write(f"{self.tab * self.count}// {line}")

## XML parsing convenience

def getElements(element, path, min = None, max = None, /):
   elements = element.findall(f"./{path}")
   if (min is not None):
      l = len(elements)
      if max is not None:
         if max < l:
            raise(Exception(f"Too many {path} elements."))
      if l < min:
         raise(Exception(f"Too few {path} elements."))
   return elements

def getElement(element, path):
   return getElements(element, path, 1, 1)[0]

def getInt(element, path):
   return int(getElement(element, path).text, base = 0)

def getOptInt(element, path, default = None, /):
   es = getElements(element, path, 0, 1)
   if es: return int(es[0].text, base = 0)
   return default

def getText(element, path):
   return getElement(element, path).text

def getOptText(element, path, default = "", /):
   es = getElements(element, path, 0, 1)
   if es: return es[0].text
   return default

## ENI parsing

def parseCoEInitCmd(element):
   if (element.get("CompleteAccess", "0") == "1"):
      ca = "TRUE"
   else:
      ca = "FALSE"
   return {
         "Comment": getOptText(element, "Comment"),
         "Transition": [t.text for t in getElements(element, "Transition")],
         "CA": ca,
         "Ccs": (0xff & getInt(element, "Ccs")),
         "Index": (0xffff & getInt(element, "Index")),
         "SubIdx": (0xff & getInt(element, "SubIndex")),
         "Timeout": (1000 * getInt(element, "Timeout")),
         "Data": list(bytearray.fromhex(getOptText(element, "Data")))
         }

def parseCoEInitCmds(elements):
   return [parseCoEInitCmd(e) for e in elements if getOptText(e, "Disabled") != "1"]

def parseSlave(element, slave):
   return {
         "Slave": (0xffff & (1 - getOptInt(element, "Info/AutoIncAddr", (1 - slave)))),
         "VendorId": (0xffffffff & getInt(element, "Info/VendorId")),
         "ProductCode": (0xffffffff & getInt(element, "Info/ProductCode")),
         "RevisionNo": (0xffffffff & getInt(element, "Info/RevisionNo")),
         "CoECmds": parseCoEInitCmds(getElements(element, "Mailbox/CoE/InitCmds/InitCmd"))
         }

def parseSlaves(element):
   es = getElements(element, "Slave")
   return [parseSlave(es[s], (s + 1)) for s in range(len(es))]

def parseConfig(element):
   return {"slave": parseSlaves(element)}

## Output generation

def genTransition(transitions):
   tt = ["ECT_ESMTRANS_" + t for t in transitions]
   if len(tt) > 1:
      return f"({' | '.join(tt)})"
   return tt[0]

def genCoEData(cg, slave, cmds):
   cName = f"s{slave}_coeData"
   size = sum(len(c["Data"]) for c in cmds)
   if size > 0:
      cg.open(f"static uint8 {cName}[{size}]")
      for data in [c["Data"] for c in cmds if len(c["Data"]) > 0]:
         cg.line(", ".join([f"{b}" for b in data]))
      cg.close()
   size = 0
   coeData = list()
   for s in [len(c["Data"]) for c in cmds]:
      if s > 0:
         offset = size
         size += s
         coeData.append((s, f"({cName} + {offset})"))
      else:
         coeData.append((0, "NULL"))
   return coeData

def genCoECmds(cg, slave, cmds):
   noCoeCmds = len(cmds)
   if noCoeCmds == 0:
      cName = "NULL"
   else:
      cName = f"s{slave}_coeCmds"
      coeData = genCoEData(cg, slave, cmds)
      cg.open(f"static ec_enicoecmdt {cName}[{noCoeCmds}]")
      for c, (DataSize, Data) in zip(cmds, coeData):
         cg.open()
         if c["Comment"]:
            cg.comment(c["Comment"])
         cg.line(f".Transition = {genTransition(c['Transition'])}")
         for f in ["CA", "Ccs"]:
            cg.line(f".{f} = {c[f]}")
         cg.line(".Index = 0x{:04X}".format(c["Index"]))
         for f in ["SubIdx", "Timeout"]:
            cg.line(f".{f} = {c[f]}")
         cg.line(f".DataSize = {DataSize}")
         cg.line(f".Data = {Data}")
         cg.close()
      cg.close()
   return (noCoeCmds, cName)

def genSlaves(cg, slaves):
   coeList = [genCoECmds(cg, s["Slave"], s["CoECmds"]) for s in slaves]
   noSlaves = sum(n > 0 for n, _ in coeList)
   if noSlaves == 0:
      cName = "NULL"
   else:
      cName = "eniSlave"
      cg.open(f"static ec_enislavet {cName}[{noSlaves}]")
      for s, (CoECmdCount, CoECmds) in zip(slaves, coeList):
         if CoECmdCount > 0:
            cg.open()
            for f in ["Slave", "VendorId", "ProductCode", "RevisionNo"]:
               cg.line(f".{f} = {s[f]}")
            cg.line(f".CoECmds = {CoECmds}")
            cg.line(f".CoECmdCount = {CoECmdCount}")
            cg.close()
      cg.close()
   return (noSlaves, cName)

def genConfig(cg, config):
   slaves = sorted(config["slave"], key = lambda s: s["Slave"])
   slavecount, slave = genSlaves(cg, slaves)
   cg.open("ec_enit ec_eni")
   cg.line(f".slave = {slave}")
   cg.line(f".slavecount = {slavecount}")
   cg.close()

def genFile(file, config):
   cg = CGen(file)
   cg.prep("include \"soem/soem.h\"")
   genConfig(cg, config)
   cg.newline()

## Interface

def parseENI(file):
   tree = ElementTree.parse(file).getroot()
   if tree.tag == "EtherCATConfig":
      path = "Config"
   else:
      path = "EtherCATConfig/Config"
   return parseConfig(getElement(tree, path))

def process(args):
   genFile(args.outfile, parseENI(args.eni))

## Program execution

def parseArgs():
   parser = argparse.ArgumentParser(
         description = "Convert an ENI file to a C file suited for an SOEM application.")
   parser.add_argument("eni", help = "the ENI file to convert",
         type = argparse.FileType("r"))
   parser.add_argument("outfile", help = "the output C file", nargs="?",
         type = argparse.FileType("w"), default = stdout)
   return parser.parse_args()

try:
   process(parseArgs())
except Exception as e:
   raise SystemExit(f"Failure: {e}.") from None

