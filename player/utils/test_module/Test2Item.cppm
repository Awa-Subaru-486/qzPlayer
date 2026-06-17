module;

#include <QQuickItem>

export module Test2Item;

export namespace qz
{
    class Test2Item : public QQuickItem
    {
        Q_OBJECT
        Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    public:
        Test2Item() {}
        ~Test2Item() override {}

        Q_INVOKABLE [[nodiscard]] QString text(const QString& str) const
        {
            return str;
        }

    public slots:
        [[nodiscard]] auto color() const -> QColor
        {
            return m_color;
        }

        auto setColor(const QColor& color) -> void
        {
            m_color = color;
        }

    signals:
        void colorChanged();

    private:
        QString m_str{"静态库测试: "};
        QColor m_color{Qt::red};
    };
}

#include "Test2Item.moc"
