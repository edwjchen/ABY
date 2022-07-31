link_directories(../../../../HyCC/src/libcircuit)

add_executable(aby-hycc main.cpp hycc_adapter.cpp circuit_converter.cpp)

target_include_directories(aby-hycc PUBLIC
  ../../../../HyCC/src/circuit-utils/include
  ../../../../HyCC/src
  ../../../../HyCC/src/cbmc/src
)

target_link_libraries(aby-hycc
  ABY::aby
  ENCRYPTO_utils::encrypto_utils
  circuit
)
