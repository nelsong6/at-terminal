// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ApplicationState.h"
#include "CascadiaSettings.h"
#include "ApplicationState.g.cpp"
#include "WindowLayout.g.cpp"
#include "ActionAndArgs.h"
#include "JsonUtils.h"
#include "FileUtils.h"
#include "../../types/inc/utils.hpp"

#include <til/io.h>

static constexpr std::wstring_view stateFileName{ L"state.json" };
static constexpr std::wstring_view elevatedStateFileName{ L"elevated-state.json" };

static constexpr std::string_view TabLayoutKey{ "tabLayout" };
static constexpr std::string_view InitialPositionKey{ "initialPosition" };
static constexpr std::string_view InitialSizeKey{ "initialSize" };
static constexpr std::string_view LaunchModeKey{ "launchMode" };

static constexpr std::wstring_view badFileSuffix{ L".bad" };

// Far past any zoom a person reaches, and small enough for the font code's math.
static constexpr float maxFontSizeDelta{ 1024.f };
static constexpr size_t maxDescribedValueLength{ 60 };

namespace Microsoft::Terminal::Settings::Model::JsonUtils
{
    using namespace winrt::Microsoft::Terminal::Settings::Model;

    template<>
    struct ConversionTrait<WindowLayout>
    {
        WindowLayout FromJson(const Json::Value& json)
        {
            auto layout = winrt::make_self<implementation::WindowLayout>();

            GetValueForKey(json, TabLayoutKey, layout->_TabLayout);
            GetValueForKey(json, InitialPositionKey, layout->_InitialPosition);
            GetValueForKey(json, LaunchModeKey, layout->_LaunchMode);
            GetValueForKey(json, InitialSizeKey, layout->_InitialSize);

            return *layout;
        }

        bool CanConvert(const Json::Value& json)
        {
            return json.isObject();
        }

        Json::Value ToJson(const WindowLayout& val)
        {
            Json::Value json{ Json::objectValue };

            SetValueForKey(json, TabLayoutKey, val.TabLayout());
            SetValueForKey(json, InitialPositionKey, val.InitialPosition());
            SetValueForKey(json, LaunchModeKey, val.LaunchMode());
            SetValueForKey(json, InitialSizeKey, val.InitialSize());

            return json;
        }

        std::string TypeDescription() const
        {
            return "WindowLayout";
        }
    };
}

using namespace ::Microsoft::Terminal::Settings::Model;

static std::wstring _describeValue(const Json::Value& value)
{
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    auto text = Json::writeString(builder, value);
    if (text.size() > maxDescribedValueLength)
    {
        text.resize(maxDescribedValueLength);
        text += "...";
    }
    return til::u8u16(text);
}

// A value that can't be read leaves its field at the default and is noted for
// the person at the screen, so one bad value costs only itself.
template<typename T>
static std::optional<T> _readField(const Json::Value& root, std::string_view key, std::vector<winrt::hstring>& problems)
{
    try
    {
        return JsonUtils::GetValueForKey<std::optional<T>>(root, key);
    }
    catch (const JsonUtils::DeserializationError& e)
    {
        problems.emplace_back(fmt::format(FMT_COMPILE(L"{}: expected {}, found {}"), til::u8u16(key), til::u8u16(e.expectedType), _describeValue(e.jsonValue)));
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
        problems.emplace_back(fmt::format(FMT_COMPILE(L"{}: holds a value that can't be read"), til::u8u16(key)));
    }
    return std::nullopt;
}

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    winrt::hstring WindowLayout::ToJson(const Model::WindowLayout& layout)
    {
        JsonUtils::ConversionTrait<Model::WindowLayout> trait;
        auto json = trait.ToJson(layout);

        Json::StreamWriterBuilder wbuilder;
        const auto content = Json::writeString(wbuilder, json);
        return hstring{ til::u8u16(content) };
    }

    Model::WindowLayout WindowLayout::FromJson(const hstring& str)
    {
        auto data = til::u16u8(str);
        std::string errs;
        std::unique_ptr<Json::CharReader> reader{ Json::CharReaderBuilder{}.newCharReader() };

        Json::Value root;
        if (!reader->parse(data.data(), data.data() + data.size(), &root, &errs))
        {
            throw winrt::hresult_error(WEB_E_INVALID_JSON_STRING, winrt::to_hstring(errs));
        }
        JsonUtils::ConversionTrait<Model::WindowLayout> trait;
        return trait.FromJson(root);
    }

    ApplicationState::ApplicationState(const std::filesystem::path& stateRoot, const bool elevated) noexcept :
        _sharedPath{ stateRoot / stateFileName },
        _elevatedPath{ stateRoot / elevatedStateFileName },
        _elevated{ elevated },
        _throttler{
            til::throttled_func_options{
                .delay = std::chrono::seconds{ 1 },
                .debounce = true,
                .trailing = true,
            },
            [this]() { _write(); }
        }
    {
        if (_read())
        {
            // The bad file has a copy now, so write it back without what was ignored.
            _throttler();
        }
    }

    // The destructor ensures that the last write is flushed to disk before returning.
    ApplicationState::~ApplicationState()
    {
        Flush();
    }

    void ApplicationState::Flush()
    {
        // This will ensure that we not just cancel the last outstanding timer,
        // but instead force it to run as soon as possible and wait for it to complete.
        _throttler.flush();
    }

    // Method Description:
    // - See GH#11119. Removes all of the data in this ApplicationState object
    //   and resets it to the defaults. This will delete the state file! That's
    //   the sure-fire way to make sure the data doesn't come back. If we leave
    //   it untouched, then when we go to write the file back out, we'll first
    //   re-read its contents and try to overlay our new state. However,
    //   nullopts won't remove keys from the JSON, so we'll end up with the
    //   original state in the file.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void ApplicationState::Reset() noexcept
    try
    {
        LOG_LAST_ERROR_IF(!DeleteFile(_sharedPath.c_str()));
        LOG_LAST_ERROR_IF(!DeleteFile(_elevatedPath.c_str()));
        *_state.lock() = {};
    }
    CATCH_LOG()

    // Deserializes the state.json and user-state (or elevated-state if
    // elevated) into this ApplicationState.
    // * ANY errors during app state will result in the creation of a new empty state.
    // * ANY errors during runtime will result in changes being partially ignored.
    // * Everything that gets ignored is held for TakeLoadProblems.
    // Returns true if a bad file was copied aside, so it's safe to rewrite it.
    bool ApplicationState::_read() const noexcept
    try
    {
        auto keptBadFile = false;

        // First get shared state out of `state.json`.
        const auto sharedData = _readSharedContents();
        if (!sharedData.empty())
        {
            // - If we're elevated, we want to only load the Shared properties
            //   from state.json. We'll then load the Local props from
            //   `elevated-state.json`
            // - If we're unelevated, then load _everything_ from state.json.
            if (_elevated)
            {
                // Only load shared properties if we're elevated
                keptBadFile = _load(_sharedPath, sharedData, FileSource::Shared);

                // Then, try and get anything in elevated-state
                if (const auto localData{ _readLocalContents() }; !localData.empty())
                {
                    keptBadFile |= _load(_elevatedPath, localData, FileSource::Local);
                }
            }
            else
            {
                // If we're unelevated, then load everything.
                keptBadFile = _load(_sharedPath, sharedData, FileSource::Shared | FileSource::Local);
            }
        }
        return keptBadFile;
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
        _state.lock()->LoadProblems.emplace_back(fmt::format(FMT_COMPILE(L"{} could not be opened, so nothing saved in it was loaded"), _sharedPath.filename().native()));
        return false;
    }

    // Loads one state file's contents. A file with problems loads whatever is
    // good in it and gets a copy beside it, which is what's left of the bad
    // values once the file is rewritten. Returns true if that copy was made.
    bool ApplicationState::_load(const std::filesystem::path& path, const std::string& data, FileSource source) const
    {
        const auto fileName = path.filename().native();
        const auto problemCount = [this]() { return _state.lock_shared()->LoadProblems.size(); };
        const auto problemsBefore = problemCount();

        std::string errs;
        std::unique_ptr<Json::CharReader> reader{ Json::CharReaderBuilder{}.newCharReader() };
        Json::Value root;

        if (!reader->parse(data.data(), data.data() + data.size(), &root, &errs))
        {
            _state.lock()->LoadProblems.emplace_back(fmt::format(FMT_COMPILE(L"{} is not valid JSON, so nothing saved in it was loaded: {}"), fileName, til::u8u16(errs)));
        }
        else if (!root.isObject())
        {
            _state.lock()->LoadProblems.emplace_back(fmt::format(FMT_COMPILE(L"{} is not a JSON object, so nothing saved in it was loaded"), fileName));
        }
        else
        {
            FromJson(root, source);
        }

        if (problemCount() == problemsBefore)
        {
            return false;
        }

        auto badPath{ path };
        badPath += badFileSuffix;
        std::error_code ec;
        std::filesystem::copy_file(path, badPath, std::filesystem::copy_options::overwrite_existing, ec);

        const auto state = _state.lock();
        if (ec)
        {
            state->LoadProblems.emplace_back(fmt::format(FMT_COMPILE(L"{} could not be copied to {}, so it was left as it is"), fileName, badPath.native()));
            return false;
        }
        state->LoadProblems.emplace_back(fmt::format(FMT_COMPILE(L"{} as it was is kept at {}"), fileName, badPath.native()));
        return true;
    }

    Windows::Foundation::Collections::IVector<hstring> ApplicationState::TakeLoadProblems()
    {
        return winrt::single_threaded_vector(std::exchange(_state.lock()->LoadProblems, {}));
    }

    // Serialized this ApplicationState (in `context`) into the state.json at _path.
    // * Errors are only logged.
    // * _state->_writeScheduled is set to false, signaling our
    //   setters that _synchronize() needs to be called again.
    void ApplicationState::_write() const noexcept
    try
    {
        Json::StreamWriterBuilder wbuilder;

        // When we're elevated, we've got to be tricky. We don't want to write
        // our window state, allowed commandlines, and other Local properties
        // into the shared `state.json`. But, if we only serialize the Shared
        // properties to a json blob, then we'll omit windowState entirely,
        // _removing_ the window state of the unelevated instance. Oh no!
        //
        // So, to be tricky, we'll first _load_ the shared state to a json blob.
        // We'll then serialize our view of the shared properties on top of that
        // blob. Then we'll write that blob back to the file. This will
        // round-trip the Local properties for the unelevated instances
        // untouched in state.json
        //
        // After that's done, we'll write our Local properties into
        // elevated-state.json.
        if (_elevated)
        {
            std::string errs;
            std::unique_ptr<Json::CharReader> reader{ Json::CharReaderBuilder{}.newCharReader() };
            Json::Value root;

            // First load the contents of state.json into a json blob. This will
            // contain the Shared properties and the unelevated instance's Local
            // properties.
            const auto sharedData = _readSharedContents();
            if (!sharedData.empty())
            {
                if (!reader->parse(sharedData.data(), sharedData.data() + sharedData.size(), &root, &errs))
                {
                    throw winrt::hresult_error(WEB_E_INVALID_JSON_STRING, winrt::to_hstring(errs));
                }
            }
            if (!root.isObject())
            {
                root = Json::Value{ Json::objectValue };
            }
            // Layer our shared properties on top of the blob from state.json,
            // and write it back out.
            _writeSharedContents(Json::writeString(wbuilder, _toJsonWithBlob(root, FileSource::Shared)));

            // Finally, write our Local properties back to elevated-state.json
            _writeLocalContents(Json::writeString(wbuilder, ToJson(FileSource::Local)));
        }
        else
        {
            // We're unelevated, this is easy. Just write everything back out.
            _writeLocalContents(Json::writeString(wbuilder, ToJson(FileSource::Local | FileSource::Shared)));
        }
    }
    CATCH_LOG()

    // Returns the application-global ApplicationState object.
    Microsoft::Terminal::Settings::Model::ApplicationState ApplicationState::SharedInstance()
    {
        static auto state = winrt::make_self<ApplicationState>(GetBaseSettingsPath());
        return *state;
    }

    // Method Description:
    // - Loads data from the given json blob. Will only read the data that's in
    //   the specified parseSource - so if we're reading the Local state file,
    //   we won't destroy previously parsed Shared data.
    // - READ: there's no layering for app state.
    void ApplicationState::FromJson(const Json::Value& root, FileSource parseSource) const noexcept
    {
        auto state = _state.lock();
        // GetValueForKey() comes in two variants:
        // * take a std::optional<T> reference
        // * return std::optional<T> by value
        // At the time of writing the former version skips missing fields in the json,
        // but we want to explicitly clear state fields that were removed from state.json.
        //
        // GH#11222: We only load properties that are of the same type (Local or
        // Shared) which we requested. If we didn't want to load this type of
        // property, just skip it.
#define MTSM_APPLICATION_STATE_GEN(source, type, name, key, ...) \
    if (WI_IsFlagSet(parseSource, source))                       \
        state->name = _readField<type>(root, key, state->LoadProblems);

        MTSM_APPLICATION_STATE_FIELDS(MTSM_APPLICATION_STATE_GEN)
#undef MTSM_APPLICATION_STATE_GEN

        // The negated test also catches NaN.
        if (state->FontSizeDelta && !(std::abs(*state->FontSizeDelta) <= maxFontSizeDelta))
        {
            state->LoadProblems.emplace_back(fmt::format(FMT_COMPILE(L"fontSizeDelta: {} is outside -{} to {}"), *state->FontSizeDelta, maxFontSizeDelta, maxFontSizeDelta));
            state->FontSizeDelta.reset();
        }
    }

    Json::Value ApplicationState::ToJson(FileSource parseSource) const noexcept
    {
        Json::Value root{ Json::objectValue };
        return _toJsonWithBlob(root, parseSource);
    }

    Json::Value ApplicationState::_toJsonWithBlob(Json::Value& root, FileSource parseSource) const noexcept
    {
        {
            const auto state = _state.lock_shared();

            // GH#11222: We only write properties that are of the same type (Local
            // or Shared) which we requested. If we didn't want to serialize this
            // type of property, just skip it.
#define MTSM_APPLICATION_STATE_GEN(source, type, name, key, ...) \
    if (WI_IsFlagSet(parseSource, source))                       \
        JsonUtils::SetValueForKey(root, key, state->name);

            MTSM_APPLICATION_STATE_FIELDS(MTSM_APPLICATION_STATE_GEN)
#undef MTSM_APPLICATION_STATE_GEN
        }
        return root;
    }

    void ApplicationState::AppendPersistedWindowLayout(Model::WindowLayout layout)
    {
        {
            const auto state = _state.lock();
            if (!state->PersistedWindowLayouts || !*state->PersistedWindowLayouts)
            {
                state->PersistedWindowLayouts = winrt::single_threaded_vector<Model::WindowLayout>();
            }
            state->PersistedWindowLayouts->Append(std::move(layout));
        }

        _throttler();
    }

    bool ApplicationState::DismissBadge(const hstring& badgeId)
    {
        bool inserted{ false };
        {
            const auto state = _state.lock();
            if (!state->DismissedBadges)
            {
                state->DismissedBadges = std::unordered_set<hstring>{};
            }
            inserted = state->DismissedBadges->insert(badgeId).second;
        }
        _throttler();
        return inserted;
    }

    bool ApplicationState::BadgeDismissed(const hstring& badgeId) const
    {
        const auto state = _state.lock_shared();
        if (state->DismissedBadges)
        {
            return state->DismissedBadges->contains(badgeId);
        }
        return false;
    }

    // Generate all getter/setters
#define MTSM_APPLICATION_STATE_GEN(source, type, name, key, ...) \
    type ApplicationState::name() const noexcept                 \
    {                                                            \
        const auto state = _state.lock_shared();                 \
        const auto& value = state->name;                         \
        return value ? *value : type{ __VA_ARGS__ };             \
    }                                                            \
                                                                 \
    void ApplicationState::name(const type& value) noexcept      \
    {                                                            \
        {                                                        \
            const auto state = _state.lock();                    \
            state->name.emplace(value);                          \
        }                                                        \
                                                                 \
        _throttler();                                            \
    }
    MTSM_APPLICATION_STATE_FIELDS(MTSM_APPLICATION_STATE_GEN)
#undef MTSM_APPLICATION_STATE_GEN

    // Method Description:
    // - Read the contents of our "shared" state - state that should be shared
    //   for elevated and unelevated instances. This is things like the list of
    //   generated profiles, the command palette commandlines.
    std::string ApplicationState::_readSharedContents() const
    {
        return til::io::read_file_as_utf8_string_if_exists(_sharedPath);
    }

    // Method Description:
    // - Read the contents of our "local" state - state that should be kept in
    //   separate files for elevated and unelevated instances. This is things
    //   like the persisted window state, and the approved commandlines (though,
    //   those don't matter when unelevated).
    // - When elevated, this will DELETE `elevated-state.json` if it has bad
    //   permissions, so we don't potentially read malicious data.
    std::string ApplicationState::_readLocalContents() const
    {
        return _elevated ?
                   til::io::read_file_as_utf8_string_if_exists(_elevatedPath, true) :
                   til::io::read_file_as_utf8_string_if_exists(_sharedPath, false);
    }

    // Method Description:
    // - Write the contents of our "shared" state - state that should be shared
    //   for elevated and unelevated instances. This will atomically write to
    //   `state.json`
    void ApplicationState::_writeSharedContents(const std::string_view content) const
    {
        til::io::write_utf8_string_to_file_atomic(_sharedPath, content);
    }

    // Method Description:
    // - Write the contents of our "local" state - state that should be kept in
    //   separate files for elevated and unelevated instances. When elevated,
    //   this will write to `elevated-state.json`, and when unelevated, this
    //   will atomically write to `user-state.json`
    void ApplicationState::_writeLocalContents(const std::string_view content) const
    {
        if (_elevated)
        {
            // DON'T use til::io::write_utf8_string_to_file_atomic, which will write to a temporary file
            // then rename that file to the final filename. That actually lets us
            // overwrite the elevate file's contents even when unelevated, because
            // we're effectively deleting the original file, then renaming a
            // different file in its place.
            //
            // We're not worried about someone else doing that though, if they do
            // that with the wrong permissions, then we'll just ignore the file and
            // start over.
            til::io::write_utf8_string_to_file(_elevatedPath, content, true);
        }
        else
        {
            til::io::write_utf8_string_to_file_atomic(_sharedPath, content);
        }
    }

}
