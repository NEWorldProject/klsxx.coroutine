cmake_minimum_required(VERSION 3.11)

project(KLSXX)

kls_add_library_module(kls.coroutine klsxx::coroutine)
kls_public_source_directory(kls.coroutine Published)
kls_module_source_directory(kls.coroutine Module)
target_link_libraries(kls.coroutine PUBLIC kls.essential kls.thread)

kls_define_tests(tests.kls.coroutine kls.coroutine Tests)
