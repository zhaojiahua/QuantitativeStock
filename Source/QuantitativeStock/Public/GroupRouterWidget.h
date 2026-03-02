// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GroupRouterWidget.generated.h"

struct FQTStockListRow;
class UStockListDownWidget;
class UItemRightClickWidget;
class UCompanyNameIndexWidget;
UCLASS()
class QUANTITATIVESTOCK_API UGroupRouterWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "QT | Params")
	FString currentDownListPath;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "QT | Params")
	int currentItemIndex = -1;
	//从一个下拉列表Copy或者剪切一只股票到指定的下拉列表里
	UFUNCTION(BlueprintCallable, Category = "QT")
	bool CopyItemToListPath(const FString& sourcePath, const FString& targetPath, int itemIndex = -1, bool isCut = false);
	//父级下拉列表组件指针,用于在被拖动时更新下拉列表的显示顺序
	UPROPERTY(BlueprintReadWrite, Category = "QT | Params")
	UStockListDownWidget* ListDownWidget_;
	//右键分组菜单
	UPROPERTY(BlueprintReadWrite, Category = "QT | Params")
	UItemRightClickWidget* RightClickWidget_;
	UPROPERTY(BlueprintReadWrite, Category = "QT | Params")
	UCompanyNameIndexWidget* companyNameIndexWidget;

private:
	TSharedPtr<FQTStockListRow> GetPathStockListItem(const FString& inFileName, int16 inIndex = -1, bool isCut = false);
	bool SaveItemToListPath(const FString& inFileName, TSharedPtr<FQTStockListRow>inItem);

};
