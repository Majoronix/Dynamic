# Dynamic

**ES** — Asistente de precisión de clics para Geometry Dash (Geode). No juega por ti: solo mueve *tus propios* clics hasta N ticks hacia adelante (nunca hacia atrás) si así evitas morir.
**EN** — Click-timing precision assist for Geometry Dash (Geode). It never plays for you: it only delays *your own* clicks by up to N ticks (never earlier) when that avoids a death.

- Geometry Dash **2.2081** · Geode SDK **5.10.1**
- Plataformas / Platforms: Windows (principal), Android, macOS (sin probar / untested)

## Ajustes / Settings
Enable · Margen 0–64 ticks · Modos asistidos (cubo, nave, bola, ufo, wave, robot, araña, swing) · Simular orbes/pads/portales · Solo wave · Solo Practice · Corregir al soltar · Ticks de vista adelante · Límite de simulación (ms) · Atajos (activar / reiniciar contador) · Indicador (posición, opacidad, contador) · Modo diagnóstico.

## Cómo funciona / How it works
Cuando pulsas o sueltas, el mod simula unos ticks con un jugador "fantasma" (estado copiado del real, mismo sistema que el Practice Mode). Elige el primer retraso (0..N) en el que el fantasma no muere. Si ninguno es seguro, tu clic se ejecuta en su momento original.

## Limitaciones / Limitations
- La simulación es una **aproximación**: simula orbes, pads y portales de tamaño/velocidad (experimental); los portales de gravedad y de vehículo están desactivados por defecto (muy experimental); ignora triggers y plataformas móviles.
- Solo asiste el botón de clic/salto (no izquierda/derecha de plataformer).
- En modo dual solo simula al jugador cuyo botón se pulsó.
- Puede considerarse trampa: úsalo solo para práctica o juego personal, no para récords o listas oficiales.

## Compilar / Build
Sube este proyecto a GitHub y ejecuta la pestaña **Actions** → *Build Dynamic (Windows)*. El `.geode` aparece en *Artifacts*.
