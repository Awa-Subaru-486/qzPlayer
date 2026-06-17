#pragma once

#include <QObject>
#include <QQuickWindow>
#include <QVulkanInstance>
#include <QQuickGraphicsDevice>
#include <QtQml/qqml.h>

#include "qzPlayer_export.hpp"

namespace qz
{

    class QZ_PLAYER_EXPORT VulkanWindowInitializer : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY(VulkanWindowInitializer)

    public:
        static VulkanWindowInitializer* instance();

        Q_INVOKABLE bool initialize(QQuickWindow* window);

        Q_INVOKABLE bool isInitialized() const;

    private:
        explicit VulkanWindowInitializer(QObject* parent = nullptr);
        ~VulkanWindowInitializer() override;

        bool m_initialized{false};
    };

}
