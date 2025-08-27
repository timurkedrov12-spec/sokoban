
#include "../inc/field.h"
#include "../inc/prog.h"

void DrawField(sf::RenderWindow& window, Field& f, sf::Font& font,
               bool hidePlayer, std::optional<Point> hideBoxAt = std::nullopt) {
    for (int y = 0; y < f.GetHeight(); ++y) {
        for (int x = 0; x < f.GetWidth(); ++x) {
            char orig = f.GetCell({x, y});
            const Point p{x, y};

            sf::RectangleShape rect({CELL_SIZE - 2.0f, CELL_SIZE - 2.0f});
            rect.setPosition(sf::Vector2f(x * CELL_SIZE + 1, y * CELL_SIZE + 1));
            rect.setFillColor(sf::Color(40, 40, 40));

            bool needText = false;
            char ch = orig;

            // 1) прятать игрока, если идёт анимация
            if (hidePlayer && orig == 'x') {
                ch = f.GetBackgroundAt(p);
            }

            // 2) прятать ТОЛЬКО тот ящик, который сейчас анимируется
            if (hideBoxAt && *hideBoxAt == Point{x, y} && orig >= 'A' && orig <= 'Z') {
                ch = f.GetBackgroundAt(p);
            }

            switch (ch) {
                case 'o':
                    rect.setFillColor(sf::Color(100, 100, 100));
                    break;  // стена
                case 'x':
                    rect.setFillColor(sf::Color::Red);
                    break;  // игрок (если не скрыли)
                default:
                    if (ch >= 'A' && ch <= 'Z') {  // статичные ящики
                        rect.setFillColor(sf::Color(0, 255, 0));
                        needText = true;
                    } else if (ch >= 'a' && ch <= 'z') {  // цели
                        rect.setFillColor(sf::Color(0, 100, 255));
                        needText = true;
                    }
                    break;
            }

            window.draw(rect);

            if (needText) {
                sf::Text label{font};
                label.setString(std::string(1, ch));
                label.setCharacterSize(CELL_SIZE * 0.7f);
                label.setFillColor(sf::Color::Black);

                const float hOff = 2.f, vOff = 8.f;
                sf::FloatRect b = label.getLocalBounds();
                label.setPosition({rect.getPosition().x + (CELL_SIZE - b.size.x) / 2 - hOff,
                                   rect.getPosition().y + (CELL_SIZE - b.size.y) / 2 - vOff});
                window.draw(label);
            }
        }
    }
}

int main() {
    try {
        int height = 14, width = 14, targets = 2, clusters = 150, moves = 5;
        auto map = generator(height, width, targets, clusters, moves);
        auto ss = MapToStringStream(map, height, width);
        Field f(ss);
        SaveFieldToFile(f, "myfile.txt");
        auto g = std::make_shared<GameDescriptor>(f);
        f.SetGame(g);
        sf::Font font;
        try {
            font = sf::Font("FunnelDisplay-VariableFont_wght.ttf");
        } catch (const sf::Exception& e) {
            std::cerr << "Ошибка загрузки шрифта: " << e.what() << std::endl;
            return 1;
        }

        StepAnim anim;
        bool showWin = false;
        bool showPause = false;
        sf::RenderWindow window(sf::VideoMode(sf::Vector2u(width * CELL_SIZE, height * CELL_SIZE)), "Sokoban SFML");
        window.setKeyRepeatEnabled(false);
        window.setVerticalSyncEnabled(true);  // sync to display refresh for smooth animation
        while (window.isOpen()) {
            while (const std::optional<sf::Event> event = window.pollEvent()) {
                if (event->is<sf::Event::Closed>()) {
                    window.close();
                }

                if (!event->is<sf::Event::KeyPressed>()) {
                    continue;
                }

                const auto* key = event->getIf<sf::Event::KeyPressed>();

                // --- PAUSE SCREEN ---
                if (showPause) {
                    switch (key->code) {
                        case (sf::Keyboard::Key::Escape): {
                            showPause = false;
                            break;
                        }
                        case (sf::Keyboard::Key::R): {
                            ReloadMap(f, "myfile.txt");
                            g = std::make_shared<GameDescriptor>(f);
                            f.SetGame(g);
                            showPause = false;
                            break;
                        }
                        case (sf::Keyboard::Key::Q): {
                            window.close();
                            break;
                        }
                        default:
                            break;
                    }
                    continue;
                }

                // --- WIN SCREEN ---
                if (showWin) {
                    switch (key->code) {
                        case (sf::Keyboard::Key::R): {
                            ReloadMap(f, "myfile.txt");
                            g = std::make_shared<GameDescriptor>(f);
                            f.SetGame(g);
                            showWin = false;
                            break;
                        }
                        case (sf::Keyboard::Key::G): {
                            try {
                                auto newMap = generator(height, width, targets, clusters, moves);
                                auto ss = MapToStringStream(newMap, height, width);
                                GenerateNewMap(f, ss);
                                g = std::make_shared<GameDescriptor>(f);
                                f.SetGame(g);
                                SaveFieldToFile(f, "myfile.txt");
                                showWin = false;
                            } catch (const std::exception& e) {
                                std::cerr << "Error generation map: " << e.what() << std::endl;
                            }
                            break;
                        }
                        case (sf::Keyboard::Key::Q): {
                            window.close();
                            break;
                        }
                        default:
                            break;
                    }
                    continue;
                }

                if (key->code == sf::Keyboard::Key::Escape) {
                    showPause = true;
                    continue;
                }

                if (!anim.isFinished()) {
                    continue;
                }

                switch (key->code) {
                    case sf::Keyboard::Key::Left:
                    case sf::Keyboard::Key::A:
                        f.Move(Directions::LEFT, anim);
                        break;
                    case sf::Keyboard::Key::Right:
                    case sf::Keyboard::Key::D:
                        f.Move(Directions::RIGHT, anim);
                        break;
                    case sf::Keyboard::Key::Up:
                    case sf::Keyboard::Key::W:
                        f.Move(Directions::UP, anim);
                        break;
                    case sf::Keyboard::Key::Down:
                    case sf::Keyboard::Key::S:
                        f.Move(Directions::DOWN, anim);
                        break;
                    default:
                        break;
                }
            }
            if (!showWin && f.HaveWon(*g)) {
                showWin = true;
            }
            window.clear(sf::Color::Black);
            if (showPause) {
                sf::Text pauseText(font, "Pause", 90);
                sf::Text chooseText(font, "Press R to restart,\n\nESC to continue,\n\nQ to quit", 22);
                pauseText.setFillColor(sf::Color::White);
                chooseText.setFillColor(sf::Color::White);
                pauseText.setPosition({150.f, window.getSize().y / 2.f - 130.f});
                chooseText.setPosition({190.f, window.getSize().y / 2.f + 10.f});
                window.draw(pauseText);
                window.draw(chooseText);
            } else if (showWin) {
                sf::Text winText(font, "You win!", 90);
                sf::Text chooseText(font, "Press R to restart,\n\nG to generate new map,\n\nQ to quit", 22);
                winText.setFillColor(sf::Color::White);
                chooseText.setFillColor(sf::Color::White);
                winText.setPosition({100.f, window.getSize().y / 2.f - 70.f});
                chooseText.setPosition({160.f, window.getSize().y / 2.f + 60.f});
                window.draw(winText);
                window.draw(chooseText);
            } else if (!anim.isFinished()) {
                // Если ящик двигается — спрячем его целевую клетку на карте,
                // чтобы не было "двойного" ящика (статический + анимируемый)
                std::optional<Point> hideBox;
                if (anim.BoxTo) {  // именно конечная точка в пикселях
                    hideBox = pxToCell(*anim.BoxTo);
                }
                DrawField(window, f, font, /*hidePlayer=*/true, /*hideBoxAt=*/hideBox);

                // игрок (анимация)
                sf::RectangleShape player({CELL_SIZE - 2.f, CELL_SIZE - 2.f});
                player.setFillColor(sf::Color::Red);
                player.setPosition(anim.playerPos());
                window.draw(player);

                // ящик (анимация)
                if (auto b = anim.boxPos()) {
                    sf::RectangleShape box({CELL_SIZE - 2.f, CELL_SIZE - 2.f});
                    box.setFillColor(sf::Color::Green);
                    box.setPosition(*b);
                    window.draw(box);

                    // (опционально) если хочешь букву на анимируемом ящике:
                    if (anim.boxChar) {  // ← проверка обязательна
                        sf::Text label{font};
                        label.setString(std::string(1, *anim.boxChar));
                        label.setCharacterSize(int(CELL_SIZE * 0.7f));
                        label.setFillColor(sf::Color::Black);
                        auto bounds = label.getLocalBounds();
                        const float hOff = 2.f, vOff = 8.f;
                        label.setPosition({box.getPosition().x + (CELL_SIZE - bounds.size.x) / 2.f - hOff,
                                           box.getPosition().y + (CELL_SIZE - bounds.size.y) / 2.f - vOff});
                        window.draw(label);
                    }
                }
            } else {
                DrawField(window, f, font, /*hidePlayer=*/false);
            }
            window.display();
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Исключение: " << e.what() << std::endl;
        return 1;
    }
}
