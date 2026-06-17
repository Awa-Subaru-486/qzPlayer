// Copyright (C) 2023-2024 Stdware Collections (https://www.github.com/stdware)
// SPDX-License-Identifier: Apache-2.0

#ifndef WINDOWAGENTBASE_H
#define WINDOWAGENTBASE_H

#include <memory>

#include <QtCore/QObject>

#include <QWKCore/qwkglobal.h>

namespace QWK {

    class WindowAgentBasePrivate;

    class QWK_CORE_EXPORT WindowAgentBase : public QObject {
        Q_OBJECT
        Q_PROPERTY(qreal aspectRatio READ aspectRatio WRITE setAspectRatio NOTIFY aspectRatioChanged RESET resetAspectRatio)
        Q_PROPERTY(bool alwaysOnTop READ alwaysOnTop WRITE setAlwaysOnTop NOTIFY alwaysOnTopChanged)
        Q_PROPERTY(bool miniMode READ miniMode WRITE setMiniMode NOTIFY miniModeChanged)
        Q_PROPERTY(int miniModeThresholdWidth READ miniModeThresholdWidth WRITE setMiniModeThresholdWidth NOTIFY miniModeThresholdWidthChanged)
        Q_PROPERTY(int miniModeThresholdHeight READ miniModeThresholdHeight WRITE setMiniModeThresholdHeight NOTIFY miniModeThresholdHeightChanged)
        Q_PROPERTY(qreal miniModeAspectRatio READ miniModeAspectRatio WRITE setMiniModeAspectRatio NOTIFY miniModeAspectRatioChanged)
        Q_DECLARE_PRIVATE(WindowAgentBase)
    public:
        ~WindowAgentBase() override;

        enum SystemButton {
            Unknown,
            WindowIcon,
            Help,
            Minimize,
            Maximize,
            Close,
        };
        Q_ENUM(SystemButton)

        qreal aspectRatio() const;
        void setAspectRatio(qreal ratio);
        void resetAspectRatio();

        bool alwaysOnTop() const;
        void setAlwaysOnTop(bool on);

        bool miniMode() const;
        void setMiniMode(bool on);

        int miniModeThresholdWidth() const;
        void setMiniModeThresholdWidth(int width);

        int miniModeThresholdHeight() const;
        void setMiniModeThresholdHeight(int height);

        qreal miniModeAspectRatio() const;
        void setMiniModeAspectRatio(qreal ratio);

        QVariant windowAttribute(const QString &key) const;
        Q_INVOKABLE bool setWindowAttribute(const QString &key, const QVariant &attribute);

    Q_SIGNALS:
        void aspectRatioChanged();
        void alwaysOnTopChanged();
        void miniModeChanged();
        void miniModeThresholdWidthChanged();
        void miniModeThresholdHeightChanged();
        void miniModeAspectRatioChanged();

    public Q_SLOTS:
        void showSystemMenu(const QPoint &pos); // Only available on Windows now
        void centralize();
        void raise();

    protected:
        explicit WindowAgentBase(WindowAgentBasePrivate &d, QObject *parent = nullptr);

        const std::unique_ptr<WindowAgentBasePrivate> d_ptr;
    };

}

#endif // WINDOWAGENTBASE_H
