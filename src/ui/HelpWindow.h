#pragma once

#include <QDialog>
#include <QPushButton>
#include <QString>
#include <QTextBrowser>

/// Language of the documentation; both files have the same content
enum class HelpLanguage { Polish, English };

/// Non-modal window with the documentation (Markdown from the resources), so the user can read it
/// while working with the scene. A button switches between the Polish and the English version.
class HelpWindow : public QDialog {
public:
    explicit HelpWindow(QWidget *parent = nullptr);

    [[nodiscard]] HelpLanguage language() const { return m_language; }
    void setLanguage(HelpLanguage language);

    /// Resource path of the documentation in the given language
    [[nodiscard]] static QString helpPath(HelpLanguage language);

private:
    void useFixedFontInCodeBlocks();

    HelpLanguage m_language = HelpLanguage::Polish;
    QTextBrowser *m_browser = nullptr;
    QPushButton *m_languageButton = nullptr;
};
