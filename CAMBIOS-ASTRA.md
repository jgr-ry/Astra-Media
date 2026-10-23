# Cambios de Astra Media respecto a VLC

Astra Media parte de VLC media player (rama `master` de
[videolan/vlc](https://github.com/videolan/vlc), commit `d3a8b45`). Como pide
la GNU GPL (sección 2a), aquí se indica qué archivos se han modificado y cuándo.
El historial de git tiene el detalle completo de cada cambio.

## 23 de octubre de 2026

### Archivos nuevos

| Archivo | Qué hace |
|---|---|
| `modules/audio_filter/astra_spectrum.c` | Analizador de espectro: calcula 48 bandas por FFT unas 60 veces por segundo sin tocar el audio |
| `modules/gui/qt/player/audio_spectrum.cpp`, `.hpp` | Recibe el espectro, lo sincroniza con lo que se oye y lo suaviza para la interfaz |
| `modules/gui/qt/player/qml/AstraSpectrumBars.qml` | Visualizador de barras redondeadas, simétrico, con picos que caen |
| `modules/gui/qt/player/qml/AstraAurora.qml` | Fondo animado hecho con los colores de la portada |
| `.github/workflows/windows.yml` | Compilación automática para Windows y publicación del ZIP |
| `extras/astra/` | Logotipo, capturas y el LEEME del paquete |
| `README.md`, `CAMBIOS-ASTRA.md` | Documentación en español (el README original pasa a `README-VLC.md`) |

### Archivos modificados

| Archivo | Cambio |
|---|---|
| `modules/gui/qt/style/systempalette.cpp`, `.hpp` | Paletas clara y oscura con colores de Apple, acento azul |
| `modules/gui/qt/style/VLCStyle.qml` | Esquinas más redondeadas, tiempos de animación y curvas con muelle |
| `modules/gui/qt/player/qml/Player.qml` | Vista de audio: portada redondeada que late, fondo vivo, visualizador y barra de control flotante |
| `modules/gui/qt/player/qml/controlbarcontrols/PlayButton.qml` | Botón de reproducir con el color de acento y animación de muelle |
| `modules/gui/qt/widgets/qml/IconToolButton.qml` | Los iconos reaccionan al pasar y pulsar |
| `modules/gui/qt/widgets/qml/StackViewExt.qml` | Transición suave entre pantallas |
| `modules/gui/qt/widgets/qml/GridItem.qml` | Las portadas de la biblioteca se elevan al pasar el ratón |
| `modules/gui/qt/maininterface/mainui.cpp` | Registra el espectro para la interfaz |
| `modules/gui/qt/maininterface/mainctx.cpp` | Sin cono navideño |
| `modules/gui/qt/qt.cpp`, `dialogs/systray/systray.cpp`, `dialogs/help/help.cpp` | Nombre «Astra Media», icono propio y aviso de marca en «Acerca de» |
| `modules/gui/qt/pixmaps/misc/cone.svg`, `pixmaps/placeholder/*.svg` | Logotipo de Astra y carátulas por defecto nuevas |
| `share/icons/**`, `share/vlc512x512.png` | Iconos de la aplicación |
| `modules/audio_filter/meson.build`, `Makefile.am`, `modules/gui/qt/meson.build`, `Makefile.am` | Compilan los archivos nuevos |
| `po/es.po`, `po/POTFILES.in` | Traducciones al español de los textos nuevos |

## 23 de octubre de 2026 (segunda tanda)

### Archivos nuevos

| Archivo | Qué hace |
|---|---|
| `modules/gui/qt/style/astra_widget_style.cpp`, `.hpp` | Estilo de las ventanas clásicas: paleta de Apple, hoja de estilos con controles redondeados, menús redondeados, botón principal azul |
| `modules/gui/qt/pixmaps/astra/*.svg` | Marcas de verificación y flechas para ese estilo |

### Archivos modificados

| Archivo | Cambio |
|---|---|
| `modules/gui/qt/maininterface/qml/NoMedialibHome.qml` | Inicio nuevo con la tarjeta «Reproduciendo ahora» y accesos rápidos |
| `modules/gui/qt/maininterface/qml/MainDisplay.qml` | El vídeo pequeño (imagen en imagen) ya no tapa la barra lateral ni la tarjeta del inicio |
| `modules/gui/qt/dialogs/preferences/*.cpp`, `.hpp` | Títulos grandes, barra de secciones y selector segmentado |
| `modules/gui/qt/pixmaps/prefs/*` | Iconos de las secciones y vistas previas nuevas |
| `modules/gui/qt/pixmaps/placeholder/*.svg` | Carátulas por defecto y botón «Añadir un servicio» rediseñados |
| `modules/gui/qt/player/audio_spectrum.*`, `qml/Player.qml`, `qml/AstraSpectrumBars.qml` | Varios visualizadores a la vez y comprobación correcta de la visibilidad de la ventana |
| `modules/gui/qt/player/qml/controlbarcontrols/ArtworkInfoWidget.qml` | Carátula de vídeo en el minirreproductor |
| `modules/audio_filter/astra_spectrum.c` | Acepta cualquier formato de audio (FLAC, WAV...) |
| `modules/gui/qt/style/systempalette.cpp` | Superficies elevadas en modo oscuro y deslizadores visibles en modo claro |
| `modules/gui/qt/qt.cpp` | Activa el estilo de las ventanas clásicas |
| `po/es.po` | Traducción completa al español |
