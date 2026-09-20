
#pragma once

#include <QKeySequence>
#include "globalinputlistener.h"

QString keyCodeToKeyString(int keyCode);

QString modifiersToString(ModifierKeys modifiers);

class ShortcutCompare
{
public:
    /**
     * @brief 静态函数：比较两个快捷键字符串是否等价（忽略大小写、空格、按键顺序）
     * @param keyStr1 第一个快捷键字符串（如"shift + ctrl + e"）
     * @param keyStr2 第二个快捷键字符串（如"Ctrl+Shift+E"）
     * @return 等价返回true，否则返回false
     */
    static bool compareShortcutKeys(const QString &keyStr1, const QString &keyStr2)
    {
        return normalizeShortcutString(keyStr1) == normalizeShortcutString(keyStr2);
    }

private:
    /**
     * @brief 内部静态函数：标准化快捷键字符串（去空格+转小写+拆分+排序）
     * @param keyStr 原始快捷键字符串（const修饰，保证不被修改）
     * @return 标准化后的按键列表
     */
    static QList<QString> normalizeShortcutString(const QString &keyStr)
    {
        // 关键修复：先创建可变副本，再操作副本（避免修改const参数）
        QString tempStr = keyStr;

        // 1. 移除所有空格（+号前后/首尾都清除）
        tempStr.remove(' '); // 用QChar(' ')比" "更高效，也避免隐式转换问题

        // 2. 全部转为小写，消除大小写差异
        tempStr = tempStr.toLower();
        // 按"+"拆分字符串，跳过空项（避免无效输入）
        QList<QString> keyParts = tempStr.split("+", Qt::SkipEmptyParts);

        // 4. 对按键列表排序，消除顺序差异
        std::sort(keyParts.begin(), keyParts.end());

        return keyParts;
    }
};
