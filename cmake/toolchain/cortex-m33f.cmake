#********************************************************************
#        _       _         _
#  _ __ | |_  _ | |  __ _ | |__   ___
# | '__|| __|(_)| | / _` || '_ \ / __|
# | |   | |_  _ | || (_| || |_) |\__ \
# |_|    \__|(_)|_| \__,_||_.__/ |___/
#
# http://www.rt-labs.com
# Copyright 2024 rt-labs AB, Sweden.
# See LICENSE file in the project root for full license information.
#*******************************************************************/

list(APPEND MACHINE
  -mcpu=cortex-m33
  -mthumb
  -mfloat-abi=hard
  -mfpu=fpv5-sp-d16
  )
