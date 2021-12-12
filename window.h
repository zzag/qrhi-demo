#pragma once

#include <QtGui/private/qrhi_p.h>
#include <QVulkanInstance>
#include <QWindow>

class Window : public QWindow
{
    Q_OBJECT

    enum class Status {
        Uninitialized,
        Initialized,
        Ready,
        Paused,
    };

public:
    explicit Window(QVulkanInstance *vulkanInstance);
    ~Window() override;

    bool event(QEvent *event) override;

protected:
    void exposeEvent(QExposeEvent *event) override;

private:
    bool resizeSwapChain();
    void releaseSwapChain();
    void releaseResources();

    void ensureReady();
    void ensurePaused();

    void initialize();
    void render();

    Status m_status = Status::Uninitialized;
    QRhi *m_rhi = nullptr;
    QRhiSwapChain *m_swapChain = nullptr;
    bool m_hasSwapChain = false;
    QRhiRenderPassDescriptor *m_renderPass = nullptr;
};

