#include <MediaMetadata.h>
#include <qdatetime.h>
#include <qtimezone.h>
#include <qimage.h>
#include <quuid.h>

#include <guiddef.h>
#include <cguid.h>
#include <mfapi.h>
#include <mfidl.h>
#include <propvarutil.h>
#include <propkey.h>

#include "private/WindowsMultimediaUtils_p.h"
#include "mfmetadata_p.h"

static const PROPERTYKEY PROP_KEY_NULL = {GUID_NULL, 0};

static QVariant convertValue(const PROPVARIANT& var)
{
    QVariant value;
    switch (var.vt) {
    case VT_LPWSTR:
        value = QString::fromUtf16(reinterpret_cast<const char16_t *>(var.pwszVal));
        break;
    case VT_UI4:
        value = uint(var.ulVal);
        break;
    case VT_UI8:
        value = qulonglong(var.uhVal.QuadPart);
        break;
    case VT_BOOL:
        value = bool(var.boolVal);
        break;
    case VT_FILETIME:
        SYSTEMTIME t;
        if (!FileTimeToSystemTime(&var.filetime, &t))
            break;

        value = QDateTime(QDate(t.wYear, t.wMonth, t.wDay),
                          QTime(t.wHour, t.wMinute, t.wSecond, t.wMilliseconds),
                          QTimeZone(QTimeZone::UTC));
        break;
    case VT_STREAM:
    {
        STATSTG stat;
        if (FAILED(var.pStream->Stat(&stat, STATFLAG_NONAME)))
            break;
        void *data = malloc(stat.cbSize.QuadPart);
        ULONG read = 0;
        if (FAILED(var.pStream->Read(data, stat.cbSize.QuadPart, &read))) {
            free(data);
            break;
        }
        value = QImage::fromData((const uchar*)data, read);
        free(data);
    }
        break;
    case VT_VECTOR | VT_LPWSTR:
        QStringList vList;
        for (ULONG i = 0; i < var.calpwstr.cElems; ++i)
            vList.append(QString::fromUtf16(reinterpret_cast<const char16_t *>(var.calpwstr.pElems[i])));
        value = vList;
        break;
    }
    return value;
}

static QVariant metaDataValue(IPropertyStore *content, const PROPERTYKEY &key)
{
    QVariant value;

    PROPVARIANT var;
    PropVariantInit(&var);
    HRESULT hr = S_FALSE;
    if (content)
        hr = content->GetValue(key, &var);

    if (SUCCEEDED(hr)) {
        value = convertValue(var);

        if (value.isValid() && content) {
            if (key == PKEY_Media_ClassPrimaryID ) {
                QString v = value.toString();
                if (v == QLatin1String("{D1607DBC-E323-4BE2-86A1-48A42A28441E}"))
                    value = QStringLiteral("Music");
                else if (v == QLatin1String("{DB9830BD-3AB3-4FAB-8A37-1A995F7FF74B}"))
                    value = QStringLiteral("Video");
                else if (v == QLatin1String("{01CD0F29-DA4E-4157-897B-6275D50C4F11}"))
                    value = QStringLiteral("Audio");
                else if (v == QLatin1String("{FCF24A76-9A57-4036-990D-E35DD8B244E1}"))
                    value = QStringLiteral("Other");
            } else if (key == PKEY_Media_Duration) {

                value = (value.toLongLong() + 10000) / 10000;
            } else if (key == PKEY_Video_Compression) {
                value = int(WindowsMultimediaUtils::codecForVideoFormat(value.toUuid()));
            } else if (key == PKEY_Audio_Format) {
                value = int(WindowsMultimediaUtils::codecForAudioFormat(value.toUuid()));
            } else if (key == PKEY_Video_FrameHeight ) {
                QSize res;
                res.setHeight(value.toUInt());
                if (content && SUCCEEDED(content->GetValue(PKEY_Video_FrameWidth, &var)))
                    res.setWidth(convertValue(var).toUInt());
                value = res;
            } else if (key == PKEY_Video_Orientation) {
                uint orientation = 0;
                if (content && SUCCEEDED(content->GetValue(PKEY_Video_Orientation, &var)))
                    orientation = convertValue(var).toUInt();
                value = orientation;
            } else if (key == PKEY_Video_FrameRate) {
                value = value.toReal() / 1000.f;
            }
        }
    }

    PropVariantClear(&var);
    return value;
}

namespace windows {

MediaMetaData MFMetaData::fromNative(IMFMediaSource* mediaSource)
{
    MediaMetaData metaData;

    IPropertyStore  *content = nullptr;
    if (!SUCCEEDED(MFGetService(mediaSource, MF_PROPERTY_HANDLER_SERVICE, IID_PPV_ARGS(&content))))
        return metaData;

    Q_ASSERT(content);
    DWORD cProps;
    if (SUCCEEDED(content->GetCount(&cProps))) {
        for (DWORD i = 0; i < cProps; i++)
        {
            PROPERTYKEY key;
            if (FAILED(content->GetAt(i, &key)))
                continue;
            MediaMetaData::Key mediaKey;
            if (key == PKEY_Author) {
                mediaKey = MediaMetaData::Author;
            } else if (key == PKEY_Title) {
                mediaKey = MediaMetaData::Title;

            } else if (key == PKEY_Media_EncodingSettings) {
                mediaKey = MediaMetaData::Description;
            } else if (key == PKEY_Copyright) {
                mediaKey = MediaMetaData::Copyright;
            } else if (key == PKEY_Comment) {
                mediaKey = MediaMetaData::Comment;
            } else if (key == PKEY_Media_ProviderStyle) {
                mediaKey = MediaMetaData::Genre;
            } else if (key == PKEY_Media_DateEncoded) {
                mediaKey = MediaMetaData::Date;

            } else if (key == PKEY_Language) {
                mediaKey = MediaMetaData::Language;
            } else if (key == PKEY_Media_Publisher) {
                mediaKey = MediaMetaData::Publisher;
            } else if (key == PKEY_Media_ClassPrimaryID) {
                mediaKey = MediaMetaData::MediaType;
            } else if (key == PKEY_Media_Duration) {
                mediaKey = MediaMetaData::Duration;
            } else if (key == PKEY_Audio_EncodingBitrate) {
                mediaKey = MediaMetaData::AudioBitRate;
            } else if (key == PKEY_Audio_Format) {
                mediaKey = MediaMetaData::AudioCodec;

            } else if (key == PKEY_Music_AlbumTitle) {
                mediaKey = MediaMetaData::AlbumTitle;
            } else if (key == PKEY_Music_AlbumArtist) {
                mediaKey = MediaMetaData::AlbumArtist;
            } else if (key == PKEY_Music_Artist) {
                mediaKey = MediaMetaData::ContributingArtist;
            } else if (key == PKEY_Music_Composer) {
                mediaKey = MediaMetaData::Composer;

            } else if (key == PKEY_Music_TrackNumber) {
                mediaKey = MediaMetaData::TrackNumber;
            } else if (key == PKEY_Music_Genre) {
                mediaKey = MediaMetaData::Genre;
            } else if (key == PKEY_ThumbnailStream) {
                mediaKey = MediaMetaData::ThumbnailImage;
            } else if (key == PKEY_Video_FrameHeight) {
                mediaKey = MediaMetaData::Resolution;
            } else if (key == PKEY_Video_Orientation) {
                mediaKey = MediaMetaData::Orientation;
            } else if (key == PKEY_Video_FrameRate) {
                mediaKey = MediaMetaData::VideoFrameRate;
            } else if (key == PKEY_Video_EncodingBitrate) {
                mediaKey = MediaMetaData::VideoBitRate;
            } else if (key == PKEY_Video_Compression) {
                mediaKey = MediaMetaData::VideoCodec;

            } else {
                continue;
            }
            metaData.insert(mediaKey, metaDataValue(content, key));
        }
    }

    content->Release();

    return metaData;
}

}

static REFPROPERTYKEY propertyKeyForMetaDataKey(MediaMetaData::Key key)
{
    switch (key) {
    case MediaMetaData::Key::Title:
        return PKEY_Title;
    case MediaMetaData::Key::Author:
        return PKEY_Author;
    case MediaMetaData::Key::Comment:
        return PKEY_Comment;
    case MediaMetaData::Key::Genre:
        return PKEY_Music_Genre;
    case MediaMetaData::Key::Copyright:
        return PKEY_Copyright;
    case MediaMetaData::Key::Publisher:
        return PKEY_Media_Publisher;
    case MediaMetaData::Key::Url:
        return PKEY_Media_AuthorUrl;
    case MediaMetaData::Key::AlbumTitle:
        return PKEY_Music_AlbumTitle;
    case MediaMetaData::Key::AlbumArtist:
        return PKEY_Music_AlbumArtist;
    case MediaMetaData::Key::TrackNumber:
        return PKEY_Music_TrackNumber;
    case MediaMetaData::Key::Date:
        return PKEY_Media_DateEncoded;
    case MediaMetaData::Key::Composer:
        return PKEY_Music_Composer;
    case MediaMetaData::Key::Duration:
        return PKEY_Media_Duration;
    case MediaMetaData::Key::Language:
        return PKEY_Language;
    case MediaMetaData::Key::Description:
        return PKEY_Media_EncodingSettings;
    case MediaMetaData::Key::AudioBitRate:
        return PKEY_Audio_EncodingBitrate;
    case MediaMetaData::Key::ContributingArtist:
        return PKEY_Music_Artist;
    case MediaMetaData::Key::ThumbnailImage:
        return PKEY_ThumbnailStream;
    case MediaMetaData::Key::Orientation:
        return PKEY_Video_Orientation;
    case MediaMetaData::Key::VideoFrameRate:
        return PKEY_Video_FrameRate;
    case MediaMetaData::Key::VideoBitRate:
        return PKEY_Video_EncodingBitrate;
    case MediaMetaData::MediaType:
        return PKEY_Media_ClassPrimaryID;
    default:
        return PROP_KEY_NULL;
    }
}

static void setStringProperty(IPropertyStore *content, REFPROPERTYKEY key, const QString &value)
{
    PROPVARIANT propValue = {};
    if (SUCCEEDED(InitPropVariantFromString(reinterpret_cast<LPCWSTR>(value.utf16()), &propValue))) {
        if (SUCCEEDED(PSCoerceToCanonicalValue(key, &propValue)))
            content->SetValue(key, propValue);
        PropVariantClear(&propValue);
    }
}

static void setUInt32Property(IPropertyStore *content, REFPROPERTYKEY key, quint32 value)
{
    PROPVARIANT propValue = {};
    if (SUCCEEDED(InitPropVariantFromUInt32(ULONG(value), &propValue))) {
        if (SUCCEEDED(PSCoerceToCanonicalValue(key, &propValue)))
            content->SetValue(key, propValue);
        PropVariantClear(&propValue);
    }
}

static void setUInt64Property(IPropertyStore *content, REFPROPERTYKEY key, quint64 value)
{
    PROPVARIANT propValue = {};
    if (SUCCEEDED(InitPropVariantFromUInt64(ULONGLONG(value), &propValue))) {
        if (SUCCEEDED(PSCoerceToCanonicalValue(key, &propValue)))
            content->SetValue(key, propValue);
        PropVariantClear(&propValue);
    }
}

static void setFileTimeProperty(IPropertyStore *content, REFPROPERTYKEY key, const FILETIME *ft)
{
    PROPVARIANT propValue = {};
    if (SUCCEEDED(InitPropVariantFromFileTime(ft, &propValue))) {
        if (SUCCEEDED(PSCoerceToCanonicalValue(key, &propValue)))
            content->SetValue(key, propValue);
        PropVariantClear(&propValue);
    }
}

namespace windows {

void MFMetaData::toNative(const MediaMetaData &metaData, IPropertyStore *content)
{
    if (content) {

        for (const auto &key : metaData.keys()) {

            QVariant value = metaData.value(key);

            if (key == MediaMetaData::Key::MediaType) {

                QString strValue = metaData.stringValue(key);
                QString v;

                if (strValue == QLatin1String("Music"))
                    v = QLatin1String("{D1607DBC-E323-4BE2-86A1-48A42A28441E}");
                else if (strValue == QLatin1String("Video"))
                    v = QLatin1String("{DB9830BD-3AB3-4FAB-8A37-1A995F7FF74B}");
                else if (strValue == QLatin1String("Audio"))
                    v = QLatin1String("{01CD0F29-DA4E-4157-897B-6275D50C4F11}");
                else
                    v = QLatin1String("{FCF24A76-9A57-4036-990D-E35DD8B244E1}");

                setStringProperty(content, PKEY_Media_ClassPrimaryID, v);

            } else if (key == MediaMetaData::Key::Duration) {

                setUInt64Property(content, PKEY_Media_Duration, value.toULongLong() * 10000);

            } else if (key == MediaMetaData::Key::Resolution) {

                QSize res = value.toSize();
                setUInt32Property(content, PKEY_Video_FrameWidth, quint32(res.width()));
                setUInt32Property(content, PKEY_Video_FrameHeight, quint32(res.height()));

            } else if (key == MediaMetaData::Key::Orientation) {

                setUInt32Property(content, PKEY_Video_Orientation, value.toUInt());

            } else if (key == MediaMetaData::Key::VideoFrameRate) {

                qreal fps = value.toReal();
                setUInt32Property(content, PKEY_Video_FrameRate, quint32(fps * 1000));

            } else if (key == MediaMetaData::Key::TrackNumber) {

                setUInt32Property(content, PKEY_Music_TrackNumber, value.toUInt());

            } else if (key == MediaMetaData::Key::AudioBitRate) {

                setUInt32Property(content, PKEY_Audio_EncodingBitrate, value.toUInt());

            } else if (key == MediaMetaData::Key::VideoBitRate) {

                setUInt32Property(content, PKEY_Video_EncodingBitrate, value.toUInt());

            } else if (key == MediaMetaData::Key::Date) {

                ULARGE_INTEGER t = {};
                t.QuadPart = ULONGLONG(value.toDateTime().toUTC().toMSecsSinceEpoch() * 10000
                                       + 116444736000000000LL);

                FILETIME ft = {};
                ft.dwHighDateTime = t.HighPart;
                ft.dwLowDateTime = t.LowPart;

                setFileTimeProperty(content, PKEY_Media_DateEncoded, &ft);

            } else {

                REFPROPERTYKEY propKey = propertyKeyForMetaDataKey(key);

                if (propKey != PROP_KEY_NULL) {
                    QString strValue = metaData.stringValue(key);
                    if (!strValue.isEmpty())
                        setStringProperty(content, propKey, strValue);
                }
            }
        }
    }
}

}
