cmake_minimum_required(VERSION 3.16)
project(crono)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Sql Xml)

# Add QXlsx library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/QXlsx/QXlsx QXlsx_build)

# Inclui diretórios necessários
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/model)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/repository)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/aggregates)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/utils)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/expected/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/QXlsx/QXlsx)

# Lista de arquivos fonte
set(SOURCES
    main.cpp
    cronometerwindow.cpp
    neweventwindow.cpp
    participantswindow.cpp
    loadparticipantswindow.cpp
    dbmanager.cpp
    report.cpp
    repository/athletes/athletesrepository.cpp
    repository/categories/categoriesrepository.cpp
    repository/modalities/modalitiesrepository.cpp
    repository/trials/trialsrepository.cpp
    repository/registrations/registrationsrepository.cpp
    repository/results/resultsrepository.cpp
    aggregates/trialaggregate.cpp
    aggregates/eventaggregate.cpp
    utils/excelutils.cpp
    utils/timeutils.cpp
)

# Lista de arquivos de cabeçalho que precisam do MOC
set(HEADERS
    cronometerwindow.h
    neweventwindow.h
    participantswindow.h
    loadparticipantswindow.h
)

# Lista de arquivos .ui
set(UI_FILES
    cronometerwindow.ui
    neweventwindow.ui
)

# Executar o MOC nos headers
qt6_wrap_cpp(MOC_SOURCES ${HEADERS})

# Executar o UIC nos arquivos .ui
qt6_wrap_ui(UI_SOURCES ${UI_FILES})

# Criar o executável
add_executable(${PROJECT_NAME} ${SOURCES} ${MOC_SOURCES} ${UI_SOURCES})

# Linkar com as bibliotecas do Qt
target_link_libraries(${PROJECT_NAME} 
    Qt6::Core 
    Qt6::Widgets 
    Qt6::Sql 
    Qt6::Xml
    QXlsx::QXlsx
)