INCLUDE(admCheckFreeType)
checkFreeType()

IF(USE_FREETYPE)
	INCLUDE(vf_plugin)

	SET(srtCommon_SRCS 
	  ADM_vidFont.cpp  ADM_vidSRT.cpp  ADM_vidSRTload.cpp  ADM_vidSRTRender.cpp  DIA_srt.cpp DIA_flySrtPos.cpp
	)

	INCLUDE(vf_plugin_gtk)
	INCLUDE(vf_plugin_qt4)

	SET(srtGtk_SRCS gtk/DIA_srt.cpp)
	SET(srtQT_SRCS  qt4/Q_srt.cpp)
	SET(srtQT_HEADERS  qt4/Q_srt.h)
	SET(srtQT_UI    qt4/srt)

	INIT_VIDEOFILTER_PLUGIN_QT4(ADM_vf_sub_qt4 
					${srtQT_SRCS} ${srtQT_HEADERS} ${srtQT_UI} ${srtCommon_SRCS})
	INIT_VIDEOFILTER_PLUGIN_GTK(  ADM_vf_sub_gtk 
					${srtGtk_SRCS} ${srtCommon_SRCS})

	ADD_DEFINITIONS(${LIBICONV_DEFINITIONS})
	ADD_SOURCE_CFLAGS(DIA_srt.cpp           "${FREETYPE2_CFLAGS}" )
	ADD_SOURCE_CFLAGS(ADM_vidSRT.cpp        "${FREETYPE2_CFLAGS}" )
	ADD_SOURCE_CFLAGS(ADM_vidFont.cpp       "${FREETYPE2_CFLAGS}" )
	ADD_SOURCE_CFLAGS(ADM_vidSRTload.cpp    "${FREETYPE2_CFLAGS}   -I${LIBICONV_INCLUDE_DIR} ")
	ADD_SOURCE_CFLAGS(ADM_vidSRTRender.cpp  "${FREETYPE2_CFLAGS}" )
	
	IF (GTK_FOUND AND GTHREAD_FOUND)
    TARGET_LINK_LIBRARIES(ADM_vf_sub_gtk PRIVATE "${FREETYPE2_LDFLAGS} ${LIBICONV_LIBRARY_DIR}")
	ENDIF (GTK_FOUND AND GTHREAD_FOUND)

	IF (QT4_FOUND)
    TARGET_LINK_LIBRARIES(ADM_vf_sub_qt4 PRIVATE "${FREETYPE2_LDFLAGS} ${LIBICONV_LIBRARY_DIR}")
	ENDIF (QT4_FOUND)
ENDIF(USE_FREETYPE)
