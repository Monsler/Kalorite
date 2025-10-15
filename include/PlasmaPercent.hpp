#pragma once
#include <kjob.h>
#include <qtmetamacros.h>

namespace Kalorite
{
    class PlasmaPercent : public KJob {
        Q_OBJECT
        
        void start() override {
            setPercent(progressPercent);
        }

        public:
        void emitPercent(const int percent);

        private:
        int progressPercent;
    };
} // namespace Kalorite
