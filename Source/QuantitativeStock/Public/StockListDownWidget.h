// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QTCurveVectorActor.h"
#include "StockListDownWidget.generated.h"

class UStockListDownItemWidget;
class UCompanyNameIndexWidget;
class UItemRightClickWidget;
UCLASS()
class QUANTITATIVESTOCK_API UStockListDownWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite,meta=(BindWidget))
	class UScrollBox* listScrollBox_;
	UPROPERTY(meta=(BindWidget))
	class UOverlay* Overlay_forRight;
	UPROPERTY(EditDefaultsOnly, Category = "QT | Params")
	TSubclassOf<UUserWidget> stockListDownItemWidget;
	UPROPERTY(EditDefaultsOnly, Category = "QT | Params")
	TSubclassOf<UUserWidget> itemRightClickWidget;
	UPROPERTY(EditInstanceOnly, Category = "QT | Params")
	UCompanyNameIndexWidget* companyNameIndexWidget;
	//从蓝图传入下拉列表路径
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite, Category = "QT | Params")
	FString currentDownListPath = TEXT("Null");
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "QT")
	UStockListDownItemWidget* GetCurrentSelectedItem();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "QT")
	UItemRightClickWidget* GetRightClickWidget();
	UFUNCTION(BlueprintImplementableEvent, Category = "QT")
	void SetAllItemOrgColor();
	//根据指定的字段对下拉列表项进行排序,默认升序排列(inIndex为0代表按照股票代码排序,1代表按照公司名称排序,2代表按照最新价排序,3代表按照开盘价排序,4代表按照最高价排序,5代表按照最低价排序,6代表按照昨日收盘价排序,7代表按照涨跌额排序,8代表按照涨跌幅排序,9代表按照换手率排序,10代表成交量排序)(ascending为0代表不排序,1代表升序,2代表降序)
	UFUNCTION(BlueprintCallable, Category = "QT")
	void SortDownListItems(int inIndex = 0, int ascending = 0);
	//排序之前的下拉列表项顺序,用于在排序之后恢复原来的顺序
	TArray<UWidget*> originalDownListItems;
	//显示下拉列表之前都必须存储一下最原始的顺序
	UFUNCTION(BlueprintCallable, Category = "QT")
	void StorageDownListItemsOrder();

	void ClearDownListItems();
	bool UpdateStockListDatas(const TArray<FQTStockRealTimeData>& listStocksDatas);
	//获取当前UI下来的股票代码
	bool GetCurrentDownListDatas(TArray<FString>& outListStocksDatas_justCode);

	//向上或向下移动ScrollBox中的DroppedWidget,movecount为正数表示向下移动，负数表示向上移动
	void SetScrollBoxItemIndex(UWidget* DroppedWidget, int16 newindex = -1, bool moveData = false);
	//移除ScrollBox中指定Index的Item
	void RemoveScrollBoxItem(int16 removeindex);
	UFUNCTION(BlueprintCallable, Category = "QT")
	void DeleteSelectedScrollBoxItem();
	UFUNCTION(BlueprintCallable, Category = "QT")
	void TopSelectedScrollBoxItem();
	
protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	//virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)override;
	virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)override;
	//virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	//virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)override;

private:

	//下拉菜单项目拖动变量
	//开始拖动Item
	bool startMove = false;
	bool firstClick = true;
	int16 itemindex = -1;//鼠标点下去时捕捉到的第itemindex个Widget
	int16 floatindex = -1;//鼠标点下去时捕捉到的第itemindex个Widget在被拖动过程中浮动的index,当鼠标移动时更新这个值
	int downListItemNum = 0;
	float itemheight = 0.0f;
	UWidget* currentSelectedItemWidget_;//当前被选中的下拉列表项
	UItemRightClickWidget* itemRight;//右键显示的功能菜单,只有这一个
	bool ctrlPressed = false;

	void SetAllScrollItemNoScale();
};
