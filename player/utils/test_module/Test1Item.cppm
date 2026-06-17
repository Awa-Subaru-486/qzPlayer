module;

#include <QQuickItem>

export module Test1Item;

export namespace qz
{
    class Test1Item : public QQuickItem
    {
        Q_OBJECT
        Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    public:
        Test1Item() {}
        ~Test1Item() override {}

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
        QColor m_color{Qt::blue};
    };
}

#include "Test1Item.moc"
