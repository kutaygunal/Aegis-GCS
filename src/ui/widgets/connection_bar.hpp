#pragma once
#include <QWidget>
#include <QLabel>

namespace aegis::ui {

class ConnectionBar : public QWidget {
    Q_OBJECT
public:
    explicit ConnectionBar(QWidget* parent = nullptr);
    void setConnected(bool connected);
private:
    QLabel* m_statusLabel;
};

} // namespace aegis::ui
