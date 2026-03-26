#ifndef SHORTCUTLAYOUTFIX_H
#define SHORTCUTLAYOUTFIX_H

#include <QObject>

class ShortcutLayoutFixFilter final : public QObject {
    Q_OBJECT

public:
    explicit ShortcutLayoutFixFilter(QObject* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};

#endif
