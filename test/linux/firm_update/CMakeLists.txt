
set(SOURCES firm_update.c)
add_executable(firm_update ${SOURCES})
target_link_libraries(firm_update soem)

if(WIN32)
  target_link_libraries(firm_update wpcap.lib Packet.lib Ws2_32.lib Winmm.lib)
elseif(UNIX)
  target_link_libraries(firm_update pthread rt)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "rt-kernel")
  target_link_libraries(firm_update "-Wl,--start-group -l${BSP} -l${ARCH} -lkern -ldev -lsio -lblock -lfs -lusb -llwip -leth -li2c -lrtc -lcan -lnand -lspi -lnor -lpwm -ladc -ltrace -lc -lm -Wl,--end-group") 
endif()

install(TARGETS firm_update DESTINATION bin)
