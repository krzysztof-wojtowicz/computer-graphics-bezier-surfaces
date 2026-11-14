#include "ui/HelpWindow.h"

#include <QDialogButtonBox>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QStringList>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QHBoxLayout>
#include <QVBoxLayout>

HelpWindow::HelpWindow(QWidget *parent) : QDialog(parent) {
    setModal(false); // read the help and rotate the scene at the same time
    resize(760, 820);

    m_browser = new QTextBrowser(this);
    m_browser->setObjectName("helpBrowser");
    m_browser->setOpenExternalLinks(true);

    // the button names the other language, in that language
    m_languageButton = new QPushButton(this);
    m_languageButton->setObjectName("helpLanguageButton");
    connect(m_languageButton, &QPushButton::clicked, this, [this] {
        setLanguage(m_language == HelpLanguage::Polish ? HelpLanguage::English : HelpLanguage::Polish);
    });

    auto *closeButtons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(closeButtons, &QDialogButtonBox::rejected, this, &QDialog::close);

    auto *bottom = new QHBoxLayout();
    bottom->addWidget(m_languageButton);
    bottom->addStretch();
    bottom->addWidget(closeButtons);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_browser);
    layout->addLayout(bottom);

    setLanguage(HelpLanguage::Polish);
}

QString HelpWindow::helpPath(const HelpLanguage language) {
    return language == HelpLanguage::Polish ? QStringLiteral(":/assets/help_pl.md")
                                            : QStringLiteral(":/assets/help_en.md");
}

void HelpWindow::setLanguage(const HelpLanguage language) {
    m_language = language;
    const bool polish = language == HelpLanguage::Polish;
    setWindowTitle(polish ? "Pomoc – Powierzchnie Béziera" : "Help – Bézier Surfaces");
    m_languageButton->setText(polish ? "English" : "Polski");

    QFile file(helpPath(language));
    if (!file.open(QIODevice::ReadOnly)) {
        // a readable message instead of an empty window
        m_browser->setPlainText("Cannot open " + file.fileName() + ": " + file.errorString());
        return;
    }
    m_browser->setMarkdown(QString::fromUtf8(file.readAll()));
    useFixedFontInCodeBlocks();
}

/// Qt's Markdown importer gives code blocks the font family "monospace", which macOS doesn't resolve:
/// the blocks were drawn in a proportional font and the columns of the matrices didn't line up
void HelpWindow::useFixedFontInCodeBlocks() {
    // the platform's fixed font first (Menlo on macOS), common ones after it, and the style hint as the last resort
    // (without a window system, e.g. in tests, the platform answers just "monospace")
    QTextCharFormat fixed;
    fixed.setFontFamilies(QStringList{QFontDatabase::systemFont(QFontDatabase::FixedFont).family(), "Menlo", "Consolas",
                                      "DejaVu Sans Mono", "Courier New"});
    fixed.setFontStyleHint(QFont::Monospace);
    fixed.setFontFixedPitch(true);

    QTextDocument *doc = m_browser->document();
    QTextCursor cursor(doc);
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        if (!block.blockFormat().hasProperty(QTextFormat::BlockCodeFence)) continue;
        cursor.setPosition(block.position());
        cursor.setPosition(block.position() + block.length() - 1, QTextCursor::KeepAnchor);
        cursor.mergeCharFormat(fixed);
    }
}
