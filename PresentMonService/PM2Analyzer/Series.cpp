#include "Series.h"
#include "MSG.h"

#include <QtWidgets/Qmenu.h>

void Series::onClicked() {

    // Create a new pen for the chart
    QPen p = pen();
    p.setWidth(p.width() + 2);
    setPen(p);
}
