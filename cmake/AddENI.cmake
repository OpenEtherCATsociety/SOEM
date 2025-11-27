# This software is dual-licensed under GPLv3 and a commercial
# license. See the file LICENSE.md distributed with this software for
# full license information.

#[=======================================================================[.rst:
add_eni
--------

Helper function to run eniconv on ENI file.

.. command:: add_eni

  .. code-block:: cmake

    add_eni(<target> <enifile>)

  Runs eniconv on ``enifile`` and adds the output file to the list of
  sources for ``<target>``.

#]=======================================================================]
find_package(Python3 REQUIRED)
find_program(ENICONV eniconv.py PATH_SUFFIXES scripts PATHS ${SOEM_SOURCE_DIR} REQUIRED)

function(add_eni target eni)
  cmake_path(GET eni STEM _eni_name)
  cmake_path(ABSOLUTE_PATH eni OUTPUT_VARIABLE _eni)

  add_custom_command (
    OUTPUT ${_eni_name}.c
    DEPENDS ${_eni}
    COMMAND ${Python3_EXECUTABLE} ${ENICONV} ${_eni} > ${_eni_name}.c
    VERBATIM
  )

  target_sources(${target}
    PRIVATE
    ${_eni_name}.c
  )
endfunction()
