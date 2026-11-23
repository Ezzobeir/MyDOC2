QT       += core gui sql network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    appointmentswidget.cpp \
    assistantdoctorsdialog.cpp \
    changepassworddialog.cpp \
    consultationsessiondialog.cpp \
    consultationwidget.cpp \
    databasemanager.cpp \
    emailservice.cpp \
    loginwindow.cpp \
    main.cpp \
    mainwindow.cpp \
    ollamaclient.cpp \
    patientdialog.cpp \
    patientfilewidget.cpp \
    patientfolder.cpp \
    preconsultationdialog.cpp \
    prescriptiondialog.cpp \
    usermanagerdialog.cpp \
    waitingroomwidget.cpp

HEADERS += \
    appointmentswidget.h \
    assistantdoctorsdialog.h \
    changepassworddialog.h \
    consultationsessiondialog.h \
    consultationwidget.h \
    databasemanager.h \
    emailservice.h \
    loginwindow.h \
    mainwindow.h \
    numberparse.h \
    ollamaclient.h \
    patientdialog.h \
    patientfilewidget.h \
    patientfolder.h \
    preconsultationdialog.h \
    prescriptiondialog.h \
    usermanagerdialog.h \
    waitingroomwidget.h

FORMS += \
    mainwindow.ui



# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
