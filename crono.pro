QT       += core gui sql xml

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++20

DEFINES += NOMINMAX

VERSION = 0.2.0.0

# Application icon
RC_ICONS = dr.ico

include(third_party/QXlsx/QXlsx/QXlsx.pri)

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += \
    $$PWD/third_party/expected/include \
    $$PWD/third_party/QXlsx/QXlsx \
    $$PWD/model


SOURCES += \
    dbmanager.cpp \
    loadparticipantswindow.cpp \
    participantswindow.cpp \
    utils/excelutils.cpp \
    utils/timeutils.cpp \
    main.cpp \
    cronometerwindow.cpp \
    neweventwindow.cpp \
    report.cpp \
    repository/athletes/athletesrepository.cpp \
    repository/categories/categoriesrepository.cpp \
    repository/modalities/modalitiesrepository.cpp \
    repository/trials/trialsrepository.cpp \
    repository/registrations/registrationsrepository.cpp \
    repository/results/resultsrepository.cpp \
    aggregates/trialaggregate.cpp \
    aggregates/eventaggregate.cpp

HEADERS += \
    cronometerwindow.h \
    dbmanager.h \
    loadparticipantswindow.h \
    participantswindow.h \
    utils/excelutils.h \
    utils/timeutils.h \
    neweventwindow.h \
    report.h \
    model/modality.h \
    model/athlete.h \
    model/category.h \
    model/trialinfo.h \
    model/registration.h \
    model/result.h \
    repository/athletes/athletesrepository.h \
    repository/modalities/modalitiesrepository.h \
    repository/categories/categoriesrepository.h \
    repository/trials/trialsrepository.h \
    repository/registrations/registrationsrepository.h \
    repository/results/resultsrepository.h \
    aggregates/trialaggregate.h \
    aggregates/eventaggregate.h

FORMS += \
    cronometerwindow.ui \
    neweventwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
