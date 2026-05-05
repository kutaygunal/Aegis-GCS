#pragma once
#include <QWidget>
#include <QLabel>
#include "core/types/common.hpp"

namespace aegis::ui {

class ConnectionBar : public QWidget {
    Q_OBJECT
public:
    explicit ConnectionBar(QWidget* parent = nullptr);
    void setConnectionState(aegis::core::types::ConnectionState state);

private:
    QLabel* m_statusLabel;
};

} // namespace aegis::ui
