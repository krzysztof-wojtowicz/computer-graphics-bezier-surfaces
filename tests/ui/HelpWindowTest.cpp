// Tests of the documentation window and of the two help files (Polish and English, the same content)

#include "ui/HelpWindow.h"

#include "renderer/MouseRotation.h"

#include <QCoreApplication>
#include <QFile>
#include <QPushButton>
#include <QRegularExpression>
#include <QStringList>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextDocument>
#include <QFontMetricsF>

#include <gtest/gtest.h>

#include <array>
#include <map>

namespace {
constexpr std::array kLanguages = {HelpLanguage::Polish, HelpLanguage::English};

const char *name(const HelpLanguage language) {
    return language == HelpLanguage::Polish ? "Polish" : "English";
}

QString readHelp(const HelpLanguage language) {
    QFile file(HelpWindow::helpPath(language));
    EXPECT_TRUE(file.open(QIODevice::ReadOnly)) << file.fileName().toStdString();
    return QString::fromUtf8(file.readAll());
}

/// Counts of the Markdown elements that must match between the two translations
std::map<std::string, int> structure(const QString &markdown) {
    std::map<std::string, int> counts;
    bool inCode = false;
    for (const QString &line : markdown.split('\n')) {
        if (line.startsWith("```")) {
            ++counts["code fences"];
            inCode = !inCode;
            continue;
        }
        if (inCode) {
            ++counts["code lines"];
            continue;
        }
        static const QRegularExpression heading("^(#+) ");
        if (const auto m = heading.match(line); m.hasMatch())
            ++counts["heading level " + std::to_string(m.captured(1).size())];
        else if (line.startsWith('|'))
            ++counts["table rows"];
        else if (line.startsWith("- ") || line.startsWith("  - "))
            ++counts["list items"];
        else if (QRegularExpression("^[0-9]+\\. ").match(line).hasMatch())
            ++counts["numbered items"];
        else if (!line.trimmed().isEmpty())
            ++counts["paragraphs"];
    }
    return counts;
}

QTextBrowser *browserOf(const HelpWindow &window) { return window.findChild<QTextBrowser *>("helpBrowser"); }
QPushButton *languageButtonOf(const HelpWindow &window) { return window.findChild<QPushButton *>("helpLanguageButton"); }
} // namespace

// ---------- the help files ----------

TEST(HelpFilesTest, BothLanguagesAreInResourcesAndNotEmpty) {
    for (const HelpLanguage language : kLanguages) {
        SCOPED_TRACE(name(language));
        const QString text = readHelp(language);
        EXPECT_GT(text.size(), 5000) << "the documentation looks truncated";
        EXPECT_TRUE(text.startsWith("# ")) << "starts with a title";
    }
    EXPECT_NE(HelpWindow::helpPath(HelpLanguage::Polish), HelpWindow::helpPath(HelpLanguage::English));
}

TEST(HelpFilesTest, BothLanguagesHaveTheSameStructure) {
    // the same content in two languages: the same headings, tables, lists, code blocks
    const auto polish = structure(readHelp(HelpLanguage::Polish));
    const auto english = structure(readHelp(HelpLanguage::English));
    EXPECT_EQ(polish, english);
    EXPECT_GE(polish.at("heading level 2"), 8);
    EXPECT_GE(polish.at("table rows"), 30);
}

TEST(HelpFilesTest, CodeBlocksAreIdenticalInBothLanguages) {
    // formulas and the file format don't depend on the language, except the words inside them
    const auto codeBlocks = [](const QString &markdown) {
        QStringList blocks;
        bool inCode = false;
        for (const QString &line : markdown.split('\n')) {
            if (line.startsWith("```")) {
                inCode = !inCode;
                if (inCode) blocks << QString();
                continue;
            }
            if (inCode) blocks.last() += line.section('(', 0, 0).trimmed() + '\n'; // without the comment in ()
        }
        return blocks;
    };
    const QStringList polish = codeBlocks(readHelp(HelpLanguage::Polish));
    const QStringList english = codeBlocks(readHelp(HelpLanguage::English));
    ASSERT_EQ(polish.size(), english.size());
    for (qsizetype i = 0; i < polish.size(); ++i) {
        // the lighting block names the light position in each language (Pświatła / Plight)
        if (polish[i].contains("Pświatła")) continue;
        EXPECT_EQ(polish[i], english[i]) << "code block " << i;
    }
}

TEST(HelpFilesTest, RequiredPartsAreThere) {
    // task, point 7: user manual with mouse controls and the list of keyboard shortcuts,
    // and a detailed technical description of turning mouse movement into rotation
    const std::map<HelpLanguage, QStringList> headings = {
        {HelpLanguage::Polish, {"## Sterowanie myszą", "## Skróty klawiaturowe", "## Opis techniczny: zamiana ruchu myszy na obrót"}},
        {HelpLanguage::English, {"## Mouse controls", "## Keyboard shortcuts", "## Technical description: from mouse movement to rotation"}},
    };
    for (const auto &[language, required] : headings) {
        SCOPED_TRACE(name(language));
        const QString text = readHelp(language);
        for (const QString &heading : required)
            EXPECT_TRUE(text.contains('\n' + heading + '\n')) << heading.toStdString();
        EXPECT_TRUE(text.contains("Ry(dx·k) · Rx(dy·k) · R")) << "the composition formula";
    }
}

TEST(HelpFilesTest, RotationSensitivityMatchesTheProgram) {
    // the documented k must be the one MouseRotation uses
    const QString k = "k = " + QString::number(static_cast<double>(kRadiansPerPixel)) + " rad/px";
    for (const HelpLanguage language : kLanguages)
        EXPECT_TRUE(readHelp(language).contains(k)) << name(language) << ": " << k.toStdString();
}

// ---------- the window ----------

TEST(HelpWindowTest, StartsInPolishAndIsNotModal) {
    const HelpWindow window;
    EXPECT_EQ(window.language(), HelpLanguage::Polish);
    EXPECT_FALSE(window.isModal()) << "the help must not block the program";
    ASSERT_NE(browserOf(window), nullptr);
    ASSERT_NE(languageButtonOf(window), nullptr);
    EXPECT_TRUE(browserOf(window)->toPlainText().contains("Sterowanie myszą"));
    EXPECT_EQ(languageButtonOf(window)->text(), "English") << "the button names the other language";
    EXPECT_TRUE(window.windowTitle().startsWith("Pomoc"));
}

TEST(HelpWindowTest, ButtonSwitchesLanguageBackAndForth) {
    HelpWindow window;
    QPushButton *button = languageButtonOf(window);
    QTextBrowser *browser = browserOf(window);
    ASSERT_NE(button, nullptr);
    ASSERT_NE(browser, nullptr);
    const QString polish = browser->toPlainText();

    button->click();
    EXPECT_EQ(window.language(), HelpLanguage::English);
    EXPECT_TRUE(browser->toPlainText().contains("Mouse controls"));
    EXPECT_FALSE(browser->toPlainText().contains("Sterowanie myszą"));
    EXPECT_EQ(button->text(), "Polski");
    EXPECT_TRUE(window.windowTitle().startsWith("Help"));

    button->click();
    EXPECT_EQ(window.language(), HelpLanguage::Polish);
    EXPECT_EQ(browser->toPlainText(), polish);
    EXPECT_EQ(button->text(), "English");
}

TEST(HelpWindowTest, MarkdownIsRenderedNotShownAsText) {
    // headings become heading blocks, "##" doesn't appear in the text; one heading block per "#" line
    for (const HelpLanguage language : kLanguages) {
        SCOPED_TRACE(name(language));
        HelpWindow window;
        window.setLanguage(language);
        const QTextDocument *doc = browserOf(window)->document();
        int headings = 0;
        for (QTextBlock block = doc->begin(); block.isValid(); block = block.next())
            if (block.blockFormat().headingLevel() > 0) ++headings;
        int expected = 0;
        for (const auto &[element, count] : structure(readHelp(language)))
            if (element.rfind("heading", 0) == 0) expected += count;
        EXPECT_EQ(headings, expected);
        EXPECT_FALSE(doc->toPlainText().contains("## "));
        EXPECT_GE(doc->toPlainText().count("Rx(θ)"), 1) << "code blocks are kept";
    }
}

TEST(HelpWindowTest, PolishCharactersSurviveTheResource) {
    // UTF-8 read as UTF-8 (a wrong codec gives "Ä…" instead of "ą")
    HelpWindow window;
    const QString text = browserOf(window)->toPlainText();
    for (const QString &word : {"Sterowanie myszą", "zwierciadlaną", "Béziera", "źródło"})
        EXPECT_TRUE(text.contains(word, Qt::CaseInsensitive)) << word.toStdString();
}

TEST(HelpWindowTest, CodeBlocksUseFixedWidthFont) {
    // the rotation matrices are aligned with spaces: in a proportional font their columns fall apart
    for (const HelpLanguage language : kLanguages) {
        SCOPED_TRACE(name(language));
        HelpWindow window;
        window.setLanguage(language);
        int codeBlocks = 0;
        for (QTextBlock block = browserOf(window)->document()->begin(); block.isValid(); block = block.next()) {
            if (!block.blockFormat().hasProperty(QTextFormat::BlockCodeFence) || block.text().isEmpty()) continue;
            ++codeBlocks;
            for (auto it = block.begin(); !it.atEnd(); ++it) {
                const QFontMetricsF metrics(it.fragment().charFormat().font());
                EXPECT_DOUBLE_EQ(metrics.horizontalAdvance("iiii"), metrics.horizontalAdvance("MMMM"))
                    << "proportional font in: " << block.text().toStdString();
            }
        }
        EXPECT_GT(codeBlocks, 10);
    }
}
