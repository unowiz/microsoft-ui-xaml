﻿// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once


using winrt::com_ptr;
using winrt::weak_ref;
using winrt::hstring;
using std::wstring_view;

namespace winrt
{
    template <typename T>
    inline hstring hstring_name_of() noexcept
    {
        return hstring(winrt::name_of<T>());
    }
}

inline winrt::IInspectable& to_winrt(::IInspectable*& instance)
{
    return reinterpret_cast<winrt::IInspectable&>(instance);
}

inline winrt::DependencyProperty InitializeDependencyProperty(
    _In_ wstring_view const& propertyNameString,
    _In_ wstring_view const& propertyTypeNameString,
    _In_ wstring_view const& ownerTypeNameString,
    bool isAttached,
    _In_opt_ winrt::IInspectable defaultValue,
    _In_opt_ winrt::PropertyChangedCallback propertyChangedCallback = nullptr)
{
    auto propertyType = winrt::Interop::TypeName();
    propertyType.Name = propertyTypeNameString;
    propertyType.Kind = winrt::Interop::TypeKind::Metadata;

    auto ownerType = winrt::Interop::TypeName();
    ownerType.Name = ownerTypeNameString;
    ownerType.Kind = winrt::Interop::TypeKind::Metadata;

    auto propertyMetadata = winrt::PropertyMetadata(defaultValue, propertyChangedCallback);

    if (isAttached)
    {
        return winrt::DependencyProperty::RegisterAttached(propertyNameString, propertyType, ownerType, propertyMetadata);
    }
    else
    {
        return winrt::DependencyProperty::Register(propertyNameString, propertyType, ownerType, propertyMetadata);
    }
}

// Helper to provide default values and boxing without differences at the call sites
template <typename T, typename Enable = void>
struct ValueHelper
{
    static T GetDefaultValue()
    {
        return T{};
    }

    static winrt::IInspectable BoxValueIfNecessary(T const& value)
    {
        return winrt::box_value(value);
    }

    static T CastOrUnbox(winrt::IInspectable const& value)
    {
        return winrt::unbox_value<T>(value);
    }

    static winrt::IInspectable BoxedDefaultValue()
    {
        return BoxValueIfNecessary(GetDefaultValue());
    }
};

template <typename T>
struct ValueHelper<T, std::enable_if_t<std::is_base_of_v<winrt::Windows::Foundation::IUnknown, T>>>
{
    static T GetDefaultValue()
    {
        return T{ nullptr };
    }

    static winrt::IInspectable BoxValueIfNecessary(T const& value)
    {
        return value;
    }

    static T CastOrUnbox(winrt::IInspectable const& value)
    {
        return value.as<T>();
    }

    static winrt::IInspectable BoxedDefaultValue()
    {
        return GetDefaultValue();
    }
};

template <>
struct ValueHelper<winrt::hstring, void>
{
    static winrt::hstring GetDefaultValue()
    {
        return winrt::hstring{ L"" };
    }

    static winrt::IInspectable BoxValueIfNecessary(winrt::hstring const& value)
    {
        return winrt::box_value(value);
    }

    static winrt::hstring CastOrUnbox(winrt::IInspectable const& value)
    {
        return winrt::unbox_value<winrt::hstring>(value);
    }

    static winrt::IInspectable BoxedDefaultValue()
    {
        return winrt::box_value(L"");
    }
};

void SetDefaultStyleKeyWorker(winrt::IControlProtected const& controlProtected, std::wstring_view const& className);

template <typename TThis>
void SetDefaultStyleKey(TThis *pThis)
{
    SetDefaultStyleKeyWorker(*pThis, pThis->GetRuntimeClassName());
}

template<typename WinRTReturn>
WinRTReturn GetTemplateChildT(std::wstring_view const& childName, const winrt::IControlProtected& controlProtected)
{
    winrt::DependencyObject childAsDO(controlProtected.GetTemplateChild(childName));

    if (childAsDO)
    {
        return childAsDO.try_as<WinRTReturn>();
    }
    return nullptr;
}

struct PropertyChanged_revoker
{
   PropertyChanged_revoker() noexcept = default;
   PropertyChanged_revoker(PropertyChanged_revoker const&) = delete;
   PropertyChanged_revoker& operator=(PropertyChanged_revoker const&) = delete;
   PropertyChanged_revoker(PropertyChanged_revoker&& other)
   {
       move_from(other);
   }

   PropertyChanged_revoker& operator=(PropertyChanged_revoker&& other)
   {
       move_from(other);
       return *this;
   }

   PropertyChanged_revoker(winrt::DependencyObject const& object, winrt::DependencyProperty const& dp, int64_t token) :
        m_object(object),
        m_property(dp),
        m_token(token)
    {}

    ~PropertyChanged_revoker() noexcept
    {
        revoke();
    }

    void revoke() noexcept
    {
        if (!m_object)
        {
            return;
        }

        if (auto object = m_object.get())
        {
            object.UnregisterPropertyChangedCallback(m_property, m_token);
        }

        m_object = nullptr;
    }

    explicit operator bool() const noexcept
    {
        return static_cast<bool>(m_object);
    }
private:
    void move_from(PropertyChanged_revoker& other)
    {
        if (this != &other)
        {
            revoke();
            m_object = other.m_object;
            m_token = other.m_token;
            m_property = other.m_property;
            other.m_object = nullptr;
            other.m_token = 0;
            other.m_property = nullptr;
        }
    }

    weak_ref<winrt::DependencyObject> m_object;
    winrt::DependencyProperty m_property{ nullptr };
    int64_t m_token{};
};

inline PropertyChanged_revoker RegisterPropertyChanged(winrt::DependencyObject const& object, winrt::DependencyProperty const& dp, winrt::DependencyPropertyChangedCallback const& callback)
{
    auto value = object.RegisterPropertyChangedCallback(dp, callback);
    return { object, dp, value };
}


// This type exists for types that in metadata derive from FrameworkElement but internally want to derive from Panel
// to get "protected" Children.
// Using it is just like any other winrt::implementation::FooT type *except* that you must pass an additional parameter
// which is the winrt::Foo type. Most times you will be using ReferenceTracker too so that pattern would look like:
// class YourType : ReferenceTracker<YourType, DeriveFromPanelHelper_base, winrt::YourType>  <--- note the extra winrt::YourType here that's normally not needed.
template <typename D, typename T, typename ... I>
struct WINRT_EBO DeriveFromPanelHelper_base : winrt::Windows::UI::Xaml::Controls::PanelT<D, winrt::default_interface<T>, winrt::composable, I...>
{
    using class_type = typename T;

    operator class_type() const noexcept
    {
        return static_cast<winrt::IInspectable>(*this).as<class_type>();
    }

    hstring GetRuntimeClassName() const
    {
        return hstring{ winrt::name_of<T>() };
    }
};