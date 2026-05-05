# TODO: Исправление ошибок IntelliSense

## Задачи:
- [ ] Исправить `cs2_internal_dll.cpp` — обернуть ImGui .cpp includes в `#ifndef __INTELLISENSE__`
- [ ] Исправить `createmove_impl.h` — добавить определения `Vec3` и `QAngle`
- [ ] Проверить результат

## Примечания:
- `__INTELLISENSE__` — макрос MSVC IntelliSense, не влияет на компиляцию
- Guard'ы предотвращают конфликты переопределения типов
