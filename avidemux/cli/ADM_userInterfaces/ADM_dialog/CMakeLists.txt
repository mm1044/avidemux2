SET(ADM_LIB ADM_dialogCli6)

SET(${ADM_LIB}_SRCS
 alert_none.cpp
#DIA_busy.cpp
        DIA_encoding.cpp
#DIA_indexing.cpp
        DIA_none.cpp
        DIA_working.cpp
        DIA_audioTracks.cpp
#OCR_none.cpp
)

ADD_LIBRARY(${ADM_LIB} STATIC ${${ADM_LIB}_SRCS})
TARGET_LINK_LIBRARIES(${ADM_LIB} PRIVATE ADM_editor6_interface adm_gettext)
