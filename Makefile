nk2edf: check_edf_file.cpp edf_annot_list.c edf_annotations.cpp nk2edf.cpp utc_date_time.c utils.c
	g++ -o nk2edf -g check_edf_file.cpp edf_annot_list.c edf_annotations.cpp nk2edf.cpp utc_date_time.c utils.c
edfconvert: check_edf_file.cpp edf_annot_list.c edf_annotations.cpp edfplusd_cnv.cpp utc_date_time.c utils.c
	g++ -o edfconvert -g check_edf_file.cpp edf_annot_list.c edf_annotations.cpp edfplusd_cnv.cpp utc_date_time.c utils.c
