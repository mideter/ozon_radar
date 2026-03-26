#include "shortcutlayoutfix.h"

#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMetaObject>

namespace {

// ЙЦУКЕН (типичная PC) → та же физическая позиция на US QWERTY (Qt::Key).
// Нужно, когда при русской раскладке в QKeyEvent приходит Unicode вместо Key_A…Key_Z.
int mapCyrillicKeyToLatin(int key)
{
    switch (key) {
    case 0x0410:
    case 0x0430:
        return 0x46; // А а → F
    case 0x0411:
    case 0x0431:
        return 0x2c; // Б б → ,
    case 0x0412:
    case 0x0432:
        return 0x44; // В в → D
    case 0x0413:
    case 0x0433:
        return 0x55; // Г г → U
    case 0x0414:
    case 0x0434:
        return 0x4c; // Д д → L
    case 0x0415:
    case 0x0435:
        return 0x54; // Е е → T
    case 0x0401:
    case 0x0451:
        return 0x60; // Ё ё → ` (Key_QuoteLeft)
    case 0x0416:
    case 0x0436:
        return 0x3b; // Ж ж → ;
    case 0x0417:
    case 0x0437:
        return 0x50; // З з → P
    case 0x0418:
    case 0x0438:
        return 0x42; // И и → B
    case 0x0419:
    case 0x0439:
        return 0x51; // Й й → Q
    case 0x041a:
    case 0x043a:
        return 0x52; // К к → R
    case 0x041b:
    case 0x043b:
        return 0x4b; // Л л → K
    case 0x041c:
    case 0x043c:
        return 0x56; // М м → V
    case 0x041d:
    case 0x043d:
        return 0x59; // Н н → Y
    case 0x041e:
    case 0x043e:
        return 0x4a; // О о → J
    case 0x041f:
    case 0x043f:
        return 0x47; // П п → G
    case 0x0420:
    case 0x0440:
        return 0x48; // Р р → H
    case 0x0421:
    case 0x0441:
        return 0x43; // С с → C
    case 0x0422:
    case 0x0442:
        return 0x4e; // Т т → N
    case 0x0423:
    case 0x0443:
        return 0x45; // У у → E
    case 0x0424:
    case 0x0444:
        return 0x41; // Ф ф → A
    case 0x0425:
    case 0x0445:
        return 0x5b; // Х х → [
    case 0x0426:
    case 0x0446:
        return 0x57; // Ц ц → W
    case 0x0427:
    case 0x0447:
        return 0x58; // Ч ч → X
    case 0x0428:
    case 0x0448:
        return 0x49; // Ш ш → I
    case 0x0429:
    case 0x0449:
        return 0x4f; // Щ щ → O
    case 0x042a:
    case 0x044a:
        return 0x5d; // Ъ ъ → ]
    case 0x042b:
    case 0x044b:
        return 0x53; // Ы ы → S
    case 0x042c:
    case 0x044c:
        return 0x4d; // Ь ь → M
    case 0x042d:
    case 0x044d:
        return 0x27; // Э э → ' (Key_Apostrophe)
    case 0x042e:
    case 0x044e:
        return 0x2e; // Ю ю → .
    case 0x042f:
    case 0x044f:
        return 0x5a; // Я я → Z
    default:
        return 0;
    }
}

bool isTextInputFocus(QObject* o)
{
    if (!o)
        return false;
    const QByteArray cn = o->metaObject()->className();
    return cn.contains("TextInput") || cn.contains("TextEdit") || cn.contains("TextField");
}

} // namespace

ShortcutLayoutFixFilter::ShortcutLayoutFixFilter(QObject* parent)
    : QObject(parent)
{
}

bool ShortcutLayoutFixFilter::eventFilter(QObject* watched, QEvent* event)
{
    Q_UNUSED(watched)
    const QEvent::Type t = event->type();
    if (t != QEvent::KeyPress && t != QEvent::KeyRelease)
        return false;

    auto* ke = static_cast<QKeyEvent*>(event);
    const Qt::KeyboardModifiers mods = ke->modifiers();
    if (!(mods & (Qt::ControlModifier | Qt::MetaModifier)))
        return false;
    // AltGr (Ctrl+Alt) — не подменяем, чтобы не ломать ввод символов.
    if ((mods & Qt::AltModifier) && (mods & Qt::ControlModifier))
        return false;

    QObject* focus = QGuiApplication::focusObject();
    if (!isTextInputFocus(focus))
        return false;

    const int key = ke->key();
    const int latin = mapCyrillicKeyToLatin(key);
    if (latin == 0 || key == latin)
        return false;

    // Пустой text — как у стандартных Ctrl+буква, иначе TextInput может не распознать шорткат.
    auto* out = new QKeyEvent(t, latin, mods, QString(), ke->isAutoRepeat(), static_cast<ushort>(ke->count()));
    QCoreApplication::postEvent(focus, out);
    return true;
}
