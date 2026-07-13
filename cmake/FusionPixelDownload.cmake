set(FUSION_PIXEL_FONT_RELEASE "2026.07.01" CACHE STRING "Pinned Fusion Pixel Font release")
set(_fusion_pixel_filename "fusion-pixel-12px-proportional-zh_hant.ttf")

if(NOT FUSION_PIXEL_FONT_FILE)
    set(_font_dir "${CMAKE_BINARY_DIR}/_deps/fusion-pixel-font")
    set(_font_extract_dir "${_font_dir}/${FUSION_PIXEL_FONT_RELEASE}")
    set(_font_archive_name
        "fusion-pixel-font-12px-proportional-ttf-v${FUSION_PIXEL_FONT_RELEASE}.zip")
    set(_font_archive "${_font_dir}/${_font_archive_name}")
    set(FUSION_PIXEL_FONT_FILE "${_font_extract_dir}/${_fusion_pixel_filename}")

    if(NOT EXISTS "${FUSION_PIXEL_FONT_FILE}")
        file(MAKE_DIRECTORY "${_font_dir}" "${_font_extract_dir}")

        if(NOT EXISTS "${_font_archive}")
            set(_font_url
                "https://github.com/TakWolf/fusion-pixel-font/releases/download/${FUSION_PIXEL_FONT_RELEASE}/${_font_archive_name}")
            message(STATUS "Downloading Fusion Pixel Font ${FUSION_PIXEL_FONT_RELEASE}")
            file(DOWNLOAD
                "${_font_url}"
                "${_font_archive}"
                STATUS _font_status
                TLS_VERIFY ON
                SHOW_PROGRESS)
            list(GET _font_status 0 _font_code)
            list(GET _font_status 1 _font_message)
            if(NOT _font_code EQUAL 0)
                file(REMOVE "${_font_archive}")
                message(FATAL_ERROR
                    "Fusion Pixel archive download failed: ${_font_message}\n"
                    "URL: ${_font_url}\n"
                    "Pass -DFUSION_PIXEL_FONT_FILE=/path/to/font.ttf for an offline build.")
            endif()
        endif()

        file(ARCHIVE_EXTRACT
            INPUT "${_font_archive}"
            DESTINATION "${_font_extract_dir}")

        if(NOT EXISTS "${FUSION_PIXEL_FONT_FILE}")
            file(GLOB_RECURSE _fusion_pixel_candidates
                LIST_DIRECTORIES FALSE
                "${_font_extract_dir}/*${_fusion_pixel_filename}")
            list(LENGTH _fusion_pixel_candidates _fusion_pixel_candidate_count)
            if(_fusion_pixel_candidate_count GREATER 0)
                list(GET _fusion_pixel_candidates 0 FUSION_PIXEL_FONT_FILE)
            else()
                message(FATAL_ERROR
                    "Fusion Pixel archive was extracted, but ${_fusion_pixel_filename} was not found.\n"
                    "Pass -DFUSION_PIXEL_FONT_FILE=/path/to/font.ttf for an offline build.")
            endif()
        endif()
    endif()
endif()

get_filename_component(FUSION_PIXEL_FONT_FILE "${FUSION_PIXEL_FONT_FILE}" ABSOLUTE)
message(STATUS "Using Fusion Pixel font: ${FUSION_PIXEL_FONT_FILE}")
