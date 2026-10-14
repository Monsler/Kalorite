#pragma once

#include <QObject>

class QQmlEngine;

namespace Kalorite {
    class AboutDialog : public QObject {
        Q_OBJECT

        public:
        explicit AboutDialog(QObject* parent);
        ~AboutDialog() override;
        void show();

        private:
        QQmlEngine* m_engine;
        QObject* m_window = nullptr;
    };
}
