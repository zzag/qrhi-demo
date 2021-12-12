#include <QGuiApplication>
#include <QVulkanInstance>

#include <QtGui/private/qrhivulkan_p.h>

#include "window.h"

int main(int argc, char *argv[])
{
    QGuiApplication a(argc, argv);

    QVulkanInstance vulkanInstance;
    vulkanInstance.setExtensions(QRhiVulkanInitParams::preferredInstanceExtensions());

#if !defined(QT_NO_DEBUG)
    vulkanInstance.setLayers(QByteArrayList{
        QByteArrayLiteral("VK_LAYER_KHRONOS_validation"),
    });
#endif

    if (!vulkanInstance.create()) {
        qFatal("Failed to initialize Vulkan instance");
    }

    Window w(&vulkanInstance);
    w.resize(800, 600);
    w.show();

    return a.exec();
}
