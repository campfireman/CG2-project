QT += widgets

qtHaveModule(printsupport): QT += printsupport

HEADERS       = imageviewer-qt5.h \
                utils/UnevenIntSpinBox.h
SOURCES       = imageviewer-qt5.cpp \
                imageviewer-main-qt5.cpp \
                utils/UnevenIntSpinBox.cpp

# install
target.path = $$[QT_INSTALL_EXAMPLES]/widgets/widgets/imageviewer
INSTALLS += target

wince*: {
   DEPLOYMENT_PLUGIN += qjpeg qgif
}
