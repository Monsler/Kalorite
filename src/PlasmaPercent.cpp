#include "PlasmaPercent.hpp"

namespace Kalorite
{
    void PlasmaPercent::emitPercent(const int percent) {
        this->progressPercent = percent;
        start();
    }
} // namespace Kalorite
