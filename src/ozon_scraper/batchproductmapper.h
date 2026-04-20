#pragma once

#include "product.h"

#include <QByteArray>
#include <QVector>


class BatchProductMapper
{
public:
    static QVector<Product> mapBatchJson(const QByteArray& batchJson, int startIndex);
};
