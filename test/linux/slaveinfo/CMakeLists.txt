
set(SOURCES slaveinfo.c)
add_executable(slaveinfo ${SOURCES})
target_link_libraries(slaveinfo soem)

if(WIN32)
  target_link_libraries(slaveinfo wpcap.lib Packet.lib Ws2_32.lib Winmm.lib)
elseif(UNIX)
  target_link_libraries(slaveinfo pthread rt)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "rt-kernel")
  target_link_libraries(slaveinfo "-Wl,--start-group -l${BSP} -l${ARCH} -lkern -ldev -lsio -lblock -lfs -lusb -llwip -leth -li2c -lrtc -lcan -lnand -lspi -lnor -lpwm -ladc -ltrace -lc -lm -Wl,--end-group") 
endif()

install(TARGETS slaveinfo DESTINATION bin)
