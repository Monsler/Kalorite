#pragma once

#include "BouncingLogo.hpp"
#include <memory>
#include <qdialog.h>
#include <qlabel.h>
#include <qwidget.h>

namespace Kalorite {
    class AboutDialog : public QDialog {
        public:
        AboutDialog(QWidget* parent);
        void invalidateLogo();

        private:
        std::shared_ptr<BouncingLogo> badge;
        std::shared_ptr<QLabel> text;
    };
}