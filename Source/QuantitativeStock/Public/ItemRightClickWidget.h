// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StockListDownWidget.h"
#include "ItemRightClickWidget.generated.h"

UCLASS()
class QUANTITATIVESTOCK_API UItemRightClickWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "QT | Params")
	FString currentDownListPath;
	UPROPERTY(BlueprintReadWrite, Category = "QT | Params")
	int currentItemIndex = -1;
	//父级下拉列表组件指针,用于在被拖动时更新下拉列表的显示顺序
	UPROPERTY(BlueprintReadWrite, Category = "QT | Params")
	UStockListDownWidget* ListDownWidget_;
	UPROPERTY(BlueprintReadWrite, Category = "QT | Params")
	class UCompanyNameIndexWidget* companyNameIndexWidget;
	//设置菜单隐藏，并同时清理右键菜单里的其他Widget
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "QT")
	void SetHidden();
protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
};
