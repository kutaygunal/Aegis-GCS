#pragma once
#include <QWidget>
#include <QLabel>

namespace aegis::ui {

class VehicleStatusBar : public QWidget {
    Q_OBJECT
public:
    explicit VehicleStatusBar(QWidget* parent = nullptr);
    void setArmed(bool armed);
    void setMode(const QString& mode);
private:
    QLabel* m_armedLabel;
    QLabel* m_modeLabel;
};

} // namespace aegis::ui
