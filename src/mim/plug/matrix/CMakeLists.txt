add_mim_plugin(matrix
    SOURCES
        matrix.cpp
        normalizers.cpp
        pass/lower_matrix_highlevel.cpp
        pass/lower_matrix_mediumlevel.cpp
        pass/lower_matrix_lowlevel.cpp
        ../compile/pass/internal_cleanup.cpp # hack
    INSTALL
)
