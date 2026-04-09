#pragma once

#include <QObject>
#include <QSettings>
#include <QString>


class SettingsService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString urlsText READ urlsText WRITE setUrlsText NOTIFY urlsTextChanged)
    Q_PROPERTY(int minPoints READ minPoints WRITE setMinPoints NOTIFY minPointsChanged)
    Q_PROPERTY(int maxPoints READ maxPoints WRITE setMaxPoints NOTIFY maxPointsChanged)

public:
    explicit SettingsService(QObject* parent = nullptr);

    QString urlsText() const;
    int minPoints() const;
    int maxPoints() const;

    Q_INVOKABLE QString minPointsText() const;
    Q_INVOKABLE QString maxPointsText() const;
    Q_INVOKABLE QString validate(const QString& urlsText,
                                 const QString& minPointsText,
                                 const QString& maxPointsText) const;
    Q_INVOKABLE bool applyAndSave(const QString& urlsText,
                                  const QString& minPointsText,
                                  const QString& maxPointsText);
    Q_INVOKABLE void resetDefaults();
    Q_INVOKABLE void load();
    Q_INVOKABLE void save();

public slots:
    void setUrlsText(const QString& urlsText);
    void setMinPoints(int minPoints);
    void setMaxPoints(int maxPoints);

signals:
    void urlsTextChanged();
    void minPointsChanged();
    void maxPointsChanged();

private:
    static constexpr int NoPointsLimit = -1;
    static constexpr int MaxReasonablePoints = 1000000;

    bool parsePointsText(const QString& text, int& outValue) const;
    bool hasAtLeastOneValidUrl(const QString& urlsText) const;
    void setDefaults();

    QSettings settings_;
    QString urlsText_;
    int minPoints_ = NoPointsLimit;
    int maxPoints_ = NoPointsLimit;
};
