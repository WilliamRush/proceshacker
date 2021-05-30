/*
 * Process Hacker -
 *   PE viewer
 *
 * Copyright (C) 2018-2021 dmex
 *
 * This file is part of Process Hacker.
 *
 * Process Hacker is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Process Hacker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Process Hacker.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <peview.h>

typedef struct _PVP_PE_PROPSTORE_CONTEXT
{
    HWND WindowHandle;
    HWND ListViewHandle;
    PH_LAYOUT_MANAGER LayoutManager;
    PPV_PROPPAGECONTEXT PropSheetContext;
} PVP_PE_PROPSTORE_CONTEXT, *PPVP_PE_PROPSTORE_CONTEXT;

VOID PvpPeEnumerateFilePropStore(
    _In_ HWND ListViewHandle
    )
{
    HRESULT status;
    IPropertyStore *propstore;
    ULONG count;
    ULONG i;

    ExtendedListView_SetRedraw(ListViewHandle, FALSE);
    ListView_DeleteAllItems(ListViewHandle);

    status = SHGetPropertyStoreFromParsingName(
        PhGetString(PvFileName),
        NULL,
        GPS_DEFAULT | GPS_EXTRINSICPROPERTIES | GPS_VOLATILEPROPERTIES,
        &IID_IPropertyStore,
        &propstore
        );

    if (status == CO_E_NOTINITIALIZED)
    {
        CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

        status = SHGetPropertyStoreFromParsingName(
            PhGetString(PvFileName),
            NULL,
            GPS_DEFAULT | GPS_EXTRINSICPROPERTIES | GPS_VOLATILEPROPERTIES,
            &IID_IPropertyStore,
            &propstore
            );
    }

    if (status == HRESULT_FROM_WIN32(ERROR_RESOURCE_TYPE_NOT_FOUND))
    {
        status = SHGetPropertyStoreFromParsingName(
            PhGetString(PvFileName),
            NULL,
            GPS_FASTPROPERTIESONLY,
            &IID_IPropertyStore,
            &propstore
            );
    }

    if (FAILED(status))
    {
        status = SHGetPropertyStoreFromParsingName(
            PhGetString(PvFileName),
            NULL,
            GPS_DEFAULT, // required for Windows 7 (dmex)
            &IID_IPropertyStore,
            &propstore
            );
    }

    if (SUCCEEDED(status))
    {
        if (SUCCEEDED(IPropertyStore_GetCount(propstore, &count)))
        {
            for (i = 0; i < count; i++)
            {
                PROPERTYKEY propkey;

                if (SUCCEEDED(IPropertyStore_GetAt(propstore, i, &propkey)))
                {
                    INT lvItemIndex;
                    PROPVARIANT propKeyVariant = { 0 };
                    PWSTR propKeyName;
                    WCHAR number[PH_INT32_STR_LEN_1];

                    PhPrintUInt32(number, i + 1);
                    lvItemIndex = PhAddListViewItem(ListViewHandle, MAXINT, number, NULL);

                    if (SUCCEEDED(PSGetNameFromPropertyKey(&propkey, &propKeyName)))
                    {
                        IPropertyDescription* propertyDescriptionPtr = NULL;

                        PhSetListViewSubItem(ListViewHandle, lvItemIndex, 1, propKeyName);

                        if (SUCCEEDED(PSGetPropertyDescriptionByName(propKeyName, &IID_IPropertyDescription, &propertyDescriptionPtr)))
                        {
                            PWSTR propertyLabel = NULL;

                            if (SUCCEEDED(IPropertyDescription_GetDisplayName(propertyDescriptionPtr, &propertyLabel)))
                            {
                                PhSetListViewSubItem(ListViewHandle, lvItemIndex, 3, propertyLabel);
                                CoTaskMemFree(propertyLabel);
                            }

                            IPropertyDescription_Release(propertyDescriptionPtr);
                        }
       
                        CoTaskMemFree(propKeyName);
                    }
                    else
                    {
                        WCHAR propKeyString[PKEYSTR_MAX];

                        if (SUCCEEDED(PSStringFromPropertyKey(&propkey, propKeyString, RTL_NUMBER_OF(propKeyString))))
                            PhSetListViewSubItem(ListViewHandle, lvItemIndex, 1, propKeyString);
                        else
                            PhSetListViewSubItem(ListViewHandle, lvItemIndex, 1, L"Unknown");
                    }

                    if (SUCCEEDED(IPropertyStore_GetValue(propstore, &propkey, &propKeyVariant)))
                    {
                        if (SUCCEEDED(PSFormatForDisplayAlloc(&propkey, &propKeyVariant, PDFF_DEFAULT, &propKeyName)))
                        {
                            PhSetListViewSubItem(ListViewHandle, lvItemIndex, 2, propKeyName);
                            CoTaskMemFree(propKeyName);
                        }

                        PropVariantClear(&propKeyVariant);
                    }
                }
            }
        }

        IPropertyStore_Release(propstore);
    }

    //ExtendedListView_SortItems(ListViewHandle);
    ExtendedListView_SetRedraw(ListViewHandle, TRUE);
}

INT_PTR CALLBACK PvpPePropStoreDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    PPVP_PE_PROPSTORE_CONTEXT context;

    if (uMsg == WM_INITDIALOG)
    {
        context = PhAllocateZero(sizeof(PVP_PE_PROPSTORE_CONTEXT));
        PhSetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT, context);

        if (lParam)
        {
            LPPROPSHEETPAGE propSheetPage = (LPPROPSHEETPAGE)lParam;
            context->PropSheetContext = (PPV_PROPPAGECONTEXT)propSheetPage->lParam;
        }
    }
    else
    {
        context = PhGetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT);
    }

    if (!context)
        return FALSE;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            context->WindowHandle = hwndDlg;
            context->ListViewHandle = GetDlgItem(hwndDlg, IDC_LIST);

            PhSetListViewStyle(context->ListViewHandle, TRUE, TRUE);
            PhSetControlTheme(context->ListViewHandle, L"explorer");
            PhAddListViewColumn(context->ListViewHandle, 0, 0, 0, LVCFMT_LEFT, 40, L"#");
            PhAddListViewColumn(context->ListViewHandle, 1, 1, 1, LVCFMT_LEFT, 150, L"Name");
            PhAddListViewColumn(context->ListViewHandle, 2, 2, 2, LVCFMT_LEFT, 250, L"Value");
            PhAddListViewColumn(context->ListViewHandle, 3, 3, 3, LVCFMT_LEFT, 150, L"Description");
            PhSetExtendedListView(context->ListViewHandle);
            PhLoadListViewColumnsFromSetting(L"ImagePropertiesListViewColumns", context->ListViewHandle);
            PvConfigTreeBorders(context->ListViewHandle);

            PhInitializeLayoutManager(&context->LayoutManager, hwndDlg);
            PhAddLayoutItem(&context->LayoutManager, context->ListViewHandle, NULL, PH_ANCHOR_ALL);

            PvpPeEnumerateFilePropStore(context->ListViewHandle);
            //ExtendedListView_SortItems(context->ListViewHandle);
            
            PhInitializeWindowTheme(hwndDlg, PeEnableThemeSupport);
        }
        break;
    case WM_DESTROY:
        {
            PhSaveListViewColumnsToSetting(L"ImagePropertiesListViewColumns", context->ListViewHandle);

            PhDeleteLayoutManager(&context->LayoutManager);

            PhFree(context);
        }
        break;
    case WM_SHOWWINDOW:
        {
            if (context->PropSheetContext && !context->PropSheetContext->LayoutInitialized)
            {
                PvAddPropPageLayoutItem(hwndDlg, hwndDlg, PH_PROP_PAGE_TAB_CONTROL_PARENT, PH_ANCHOR_ALL);
                PvDoPropPageLayout(hwndDlg);

                context->PropSheetContext->LayoutInitialized = TRUE;
            }
        }
        break;
    case WM_SIZE:
        {
            PhLayoutManagerLayout(&context->LayoutManager);
        }
        break;
    case WM_NOTIFY:
        {
            PvHandleListViewNotifyForCopy(lParam, context->ListViewHandle);
        }
        break;
    case WM_CONTEXTMENU:
        {
            PvHandleListViewCommandCopy(hwndDlg, lParam, wParam, context->ListViewHandle);
        }
        break;
    }

    return FALSE;
}