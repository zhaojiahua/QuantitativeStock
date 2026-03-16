
#include "StockListDownItemWidget.h"
#include "Blueprint/DragDropOperation.h"

bool UStockListDownItemWidget::CompareTo(const UStockListDownItemWidget& Other, int inIndex, bool ascending) const {
    // 假设有成员变量FQTStockRealTimeData StockRealTimeData_;
    // 按inIndex比较StockData的不同字段(inIndex为0代表按照股票代码排序,1代表按照公司名称排序,2代表按照最新价排序,3代表按照开盘价排序,4代表按照最高价排序,5代表按照最低价排序,6代表按照昨日收盘价排序,7代表按照涨跌额排序,8代表按照涨跌幅排序,9代表按照换手率排序,10代表成交量排序)
    switch (inIndex) {
    case 0: // 股票代码
        return ascending ? StockRealTimeData_.StockCode < Other.StockRealTimeData_.StockCode : StockRealTimeData_.StockCode > Other.StockRealTimeData_.StockCode;
    case 1: // 公司名称
        return ascending ? StockRealTimeData_.StockName < Other.StockRealTimeData_.StockName : StockRealTimeData_.StockName > Other.StockRealTimeData_.StockName;
    case 2: // 最新价
        return ascending ? StockRealTimeData_.LatestPrice < Other.StockRealTimeData_.LatestPrice : StockRealTimeData_.LatestPrice > Other.StockRealTimeData_.LatestPrice;
    case 3: // 开盘价
        return ascending ? StockRealTimeData_.OpenPrice < Other.StockRealTimeData_.OpenPrice : StockRealTimeData_.OpenPrice > Other.StockRealTimeData_.OpenPrice;
    case 4: // 最高价
        return ascending ? StockRealTimeData_.HighestPrice < Other.StockRealTimeData_.HighestPrice : StockRealTimeData_.HighestPrice > Other.StockRealTimeData_.HighestPrice;
    case 5: // 最低价
        return ascending ? StockRealTimeData_.LowestPrice < Other.StockRealTimeData_.LowestPrice : StockRealTimeData_.LowestPrice > Other.StockRealTimeData_.LowestPrice;
    case 6: // 昨收价
        return ascending ? StockRealTimeData_.PreviousClosePrice < Other.StockRealTimeData_.PreviousClosePrice : StockRealTimeData_.PreviousClosePrice > Other.StockRealTimeData_.PreviousClosePrice;
    case 7: // 涨跌额
        return ascending ? StockRealTimeData_.ChangeAmount < Other.StockRealTimeData_.ChangeAmount : StockRealTimeData_.ChangeAmount > Other.StockRealTimeData_.ChangeAmount;
    case 8: // 涨跌幅
        return ascending ? StockRealTimeData_.ChangeRatio < Other.StockRealTimeData_.ChangeRatio : StockRealTimeData_.ChangeRatio > Other.StockRealTimeData_.ChangeRatio;
    case 9: // 换手率
        return ascending ? StockRealTimeData_.TurnoverRate < Other.StockRealTimeData_.TurnoverRate : StockRealTimeData_.TurnoverRate > Other.StockRealTimeData_.TurnoverRate;
    case 10: // 成交量
        return ascending ? StockRealTimeData_.Volume < Other.StockRealTimeData_.Volume : StockRealTimeData_.Volume > Other.StockRealTimeData_.Volume;
    default:
        return false; // 默认不排序
    }
}
