# AMRNB decoder
INCLUDE(admCheckAmrnb)
checkAmrnb()

IF(AMRNB_FOUND)
  INCLUDE(ad_plugin)

  SET(ADM_ad_amrnb_SRCS ADM_ad_amrnb.cpp)
  ADD_AUDIO_DECODER(ADM_ad_amrnb ${ADM_ad_amrnb_SRCS})
  TARGET_LINK_LIBRARIES(ADM_ad_amrnb PRIVATE ${AMRNB_LIBRARY_DIR})

  ADD_TARGET_CFLAGS(ADM_ad_amrnb "-I${AMRNB_INCLUDE_DIR}")

  INIT_AUDIO_PLUGIN(ADM_ad_amrnb)
  INSTALL_AUDIODECODER(ADM_ad_amrnb)
ENDIF()
