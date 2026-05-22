#include <QApplication>
#include "HCNetSDK.h"
#include "LoginDialog.h"
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("VisionPlayback");

    NET_DVR_Init();
    NET_DVR_SetLogToFile(3, (char*)"./sdkLog/");

    LoginDialog dlg;
    if (dlg.exec() != QDialog::Accepted) {
        NET_DVR_Cleanup();
        return 0;
    }

    MainWindow w;
    w.setUserId(dlg.userId());
    w.setWindowTitle("VisionPlayback");
    w.resize(960, 600);
    w.show();

    int ret = app.exec();
    // MainWindow destructor calls NET_DVR_Logout + NET_DVR_Cleanup
    return ret;
}
