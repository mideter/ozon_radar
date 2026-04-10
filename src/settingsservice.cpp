#include "settingsservice.h"

#include <QStringList>
#include <QUrl>

namespace {
constexpr auto kUrlsTextKey = "settings/urlsText";
constexpr auto kMinPointsKey = "settings/minPoints";
constexpr auto kMaxPointsKey = "settings/maxPoints";

const QString kDefaultUrl =
    QStringLiteral("https://www.ozon.ru/category/aksessuary-i-prinadlezhnosti-dlya-rybalki-11340/?category_was_predicted=true&deny_category_prediction=true&from_global=true&has_points_from_reviews=t&sorting=price&text=коромысло");
}

SettingsService::SettingsService(QObject* parent)
    : QObject(parent)
    , settings_()
{
    setDefaults();
    load();
}

QString SettingsService::urlsText() const
{
    return urlsText_;
}

int SettingsService::minPoints() const
{
    return minPoints_;
}

int SettingsService::maxPoints() const
{
    return maxPoints_;
}

QString SettingsService::minPointsText() const
{
    return minPoints_ >= 0 ? QString::number(minPoints_) : QString();
}

QString SettingsService::maxPointsText() const
{
    return maxPoints_ >= 0 ? QString::number(maxPoints_) : QString();
}

QString SettingsService::validate(const QString& urlsText,
                                  const QString& minPointsText,
                                  const QString& maxPointsText) const
{
    if (!hasAtLeastOneValidUrl(urlsText))
        return QStringLiteral("Укажите хотя бы одну корректную ссылку.");

    int minValue = NoPointsLimit;
    int maxValue = NoPointsLimit;
    if (!parsePointsText(minPointsText, minValue))
        return QStringLiteral("Минимальные баллы должны быть целым числом >= 0.");
    if (!parsePointsText(maxPointsText, maxValue))
        return QStringLiteral("Максимальные баллы должны быть целым числом >= 0.");
    if (minValue >= 0 && maxValue >= 0 && minValue > maxValue)
        return QStringLiteral("Минимальные баллы не могут быть больше максимальных.");

    return QString();
}

bool SettingsService::applyAndSave(const QString& urlsText,
                                   const QString& minPointsText,
                                   const QString& maxPointsText)
{
    const QString validationError = validate(urlsText, minPointsText, maxPointsText);
    if (!validationError.isEmpty())
        return false;

    int minValue = NoPointsLimit;
    int maxValue = NoPointsLimit;
    parsePointsText(minPointsText, minValue);
    parsePointsText(maxPointsText, maxValue);

    setUrlsText(urlsText);
    setMinPoints(minValue);
    setMaxPoints(maxValue);
    save();
    return true;
}

void SettingsService::load()
{
    const QString loadedUrls = settings_.value(kUrlsTextKey, kDefaultUrl).toString();
    setUrlsText(loadedUrls);

    int loadedMin = settings_.value(kMinPointsKey, NoPointsLimit).toInt();
    int loadedMax = settings_.value(kMaxPointsKey, NoPointsLimit).toInt();
    if (loadedMin < NoPointsLimit || loadedMin > MaxReasonablePoints)
        loadedMin = NoPointsLimit;
    if (loadedMax < NoPointsLimit || loadedMax > MaxReasonablePoints)
        loadedMax = NoPointsLimit;
    if (loadedMin >= 0 && loadedMax >= 0 && loadedMin > loadedMax) {
        loadedMin = NoPointsLimit;
        loadedMax = NoPointsLimit;
    }
    setMinPoints(loadedMin);
    setMaxPoints(loadedMax);
}

void SettingsService::save()
{
    settings_.setValue(kUrlsTextKey, urlsText_);
    settings_.setValue(kMinPointsKey, minPoints_);
    settings_.setValue(kMaxPointsKey, maxPoints_);
    settings_.sync();
}

void SettingsService::setUrlsText(const QString& urlsText)
{
    if (urlsText_ == urlsText)
        return;
    urlsText_ = urlsText;
    emit urlsTextChanged();
}

void SettingsService::setMinPoints(int minPoints)
{
    if (minPoints < NoPointsLimit)
        minPoints = NoPointsLimit;
    if (minPoints > MaxReasonablePoints)
        minPoints = MaxReasonablePoints;
    if (minPoints_ == minPoints)
        return;
    minPoints_ = minPoints;
    emit minPointsChanged();
}

void SettingsService::setMaxPoints(int maxPoints)
{
    if (maxPoints < NoPointsLimit)
        maxPoints = NoPointsLimit;
    if (maxPoints > MaxReasonablePoints)
        maxPoints = MaxReasonablePoints;
    if (maxPoints_ == maxPoints)
        return;
    maxPoints_ = maxPoints;
    emit maxPointsChanged();
}

bool SettingsService::parsePointsText(const QString& text, int& outValue) const
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        outValue = NoPointsLimit;
        return true;
    }
    bool ok = false;
    const int parsed = trimmed.toInt(&ok);
    if (!ok || parsed < 0 || parsed > MaxReasonablePoints)
        return false;
    outValue = parsed;
    return true;
}

bool SettingsService::hasAtLeastOneValidUrl(const QString& urlsText) const
{
    const QStringList rawLines = urlsText.split(QChar('\n'));
    for (QString line : rawLines) {
        line = line.trimmed();
        if (line.isEmpty())
            continue;
        if (!line.startsWith(QLatin1String("http://"), Qt::CaseInsensitive)
            && !line.startsWith(QLatin1String("https://"), Qt::CaseInsensitive)) {
            line = QStringLiteral("https://") + line;
        }
        const QUrl url = QUrl::fromUserInput(line);
        if (url.isValid() && !url.scheme().isEmpty() && !url.host().isEmpty())
            return true;
    }
    return false;
}

void SettingsService::setDefaults()
{
    urlsText_ = kDefaultUrl;
    minPoints_ = NoPointsLimit;
    maxPoints_ = NoPointsLimit;
}
