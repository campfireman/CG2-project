QT += widgets

qtHaveModule(printsupport): QT += printsupport

HEADERS       = imageviewer-qt5.h \
                utils/QUnevenIntSpinBox.h
SOURCES       = imageviewer-qt5.cpp \
                imageviewer-main-qt5.cpp \
                utils/QUnevenIntSpinBox.cpp

# install
target.path = $$[QT_INSTALL_EXAMPLES]/widgets/widgets/imageviewer
INSTALLS += target

wince*: {
   DEPLOYMENT_PLUGIN += qjpeg qgif
}
