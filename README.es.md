# Frenchcore Machine

[English](README.md)

Un sintetizador wavetable en formato plugin con un solo objetivo: hacer kicks de frenchcore rápido.

Frenchcore Machine está a mitad de camino entre arrastrar un sample y construir un kick desde cero en un synth completo. Trae dos osciladores wavetable, una envolvente que ya tiene la forma clásica de kick + bass, una capa de click (el "tock") y una cadena de efectos reordenable con lo que un kick de frenchcore realmente necesita. Pensalo como un Serum o un Vital muy simplificado, enfocado en kicks.

> **Estado:** la versión 0.1.0 es la última publicada (se probó solo en FL Studio). La versión 0.2.0 también está publicada, y la versión 0.3.0 está construida y espera revisión y publicación: ver el [plan](docs/PLAN.md) y el [changelog](CHANGELOG.md) (en inglés).

## Funciones

Novedades de la v0.3.0:

- Cinco efectos nuevos: **Peak** (una zona de frecuencias reforzada que pasa por un recortador de suave a duro, con un pad XY), **Formant** (un filtro de vocales que hace "hablar" a la cola), **Phaser**, **Flanger** y **Volume LFO**.
- El Phaser y el Flanger barren con el tempo (o en Hz) y arrancan de nuevo en cada nota, así que todos los kicks suenan igual; con Key Track el tono metálico del Flanger sigue a la nota.
- El Volume LFO es un modulador de volumen al estilo de LFOTool: dibujás la forma en un editor que se abre dentro de la caja de Efectos, elegís una de diez formas listas (Pump, Sidechain, Gates, Sierras, Triángulo, Seno, Cuadrada) y lo usás para bombeo o gate después de la distorsión y la reverb.
- Los proyectos y presets de la 0.1 y la 0.2 se abren y suenan igual (lo comprueban los tests); un proyecto guardado con la 0.3 se abre en la 0.2 sin los efectos nuevos.

De la v0.2.0:

- Un mixer (Osc 1, Osc 2, Noise, Tock) con Level, Mute, Solo y un destino para cada sonido: Main, Bus A o Bus B. Cada bus tiene su propia fila de efectos y puede actuar sobre todo el kick, solo el punch o solo la cola.
- Hasta cuatro copias de cada efecto y ocho efectos por fila; agregar, duplicar y quitar desde la ventana.
- Efectos nuevos: Filter, Transient shaper, Compressor (con un modo multibanda estilo OTT) y Reverb. El EQ suma bandas Extreme (hasta +-36 dB y Q 50) y Key Track.
- Un oscilador de ruido: ocho ruidos generados o tu propio WAV.
- Un Tock renovado: volumen propio (interruptor Amp Env), forma de onda con manijas de recorte y Detect, Pogo (curva de pitch) y samples de hasta 10 segundos.
- Los proyectos y presets de la 0.1 se abren y suenan igual (lo comprueban los tests); un proyecto guardado con la 0.2 no se abre bien en la 0.1.

De la v0.1.0:

- Dos osciladores wavetable con wavetables de fábrica para kicks, y carga de wavetables propias (WAV compatible con Serum/Vital).
- Envolvente de amplitud editable (forma kick + bass por defecto) y envolvente de pitch, con puntos y perillas.
- Tock: una capa opcional de click, sintetizada o con tocks de fábrica o tus propios samples.
- Niveles separados para el punch y la cola, con punto de división ajustable.
- Monofónico con glide opcional; la nota MIDI afina la cola del kick.
- Efectos que podés ordenar arrastrándolos: distorsión (soft clip, hard clip, overdrive, foldback, bitcrush), EQ paramétrico de 4 bandas y stereo multibanda (en la 0.1; la 0.2 suma más).
- Presets: cinco de fábrica, y guardás los tuyos.
- VST3, AU y Standalone para Windows, macOS y Linux.

La lista completa está en el [MVP scope](docs/MVP_SCOPE.md).

## Descarga

La versión 0.1.0 está en la [página de Releases](https://github.com/nocturn444/frenchcore-machine/releases/latest): un zip para Windows, macOS y Linux. Los pasos de instalación, con los avisos de la primera vez por los plugins sin firmar, están en la [guía de usuario](docs/USER_GUIDE.md#installation) (en inglés). Las versiones de desarrollo se adjuntan a cada ejecución en la pestaña Actions.

## Compilar desde el código

Ver [docs/BUILDING.md](docs/BUILDING.md).

## Documentación

Toda la documentación está en [docs/](docs/README.md), en inglés. Para empezar:

- [Plan de desarrollo](docs/PLAN.md)
- [Arquitectura](docs/ARCHITECTURE.md)
- [Glosario](docs/GLOSSARY.md) con los términos técnicos

## Contribuir

Las contribuciones son bienvenidas. Leé [CONTRIBUTING.md](CONTRIBUTING.md) antes: el proyecto sigue desarrollo guiado por documentación, así que cada función empieza con una especificación escrita.

## Seguridad

Para avisar de una vulnerabilidad, usá el reporte privado de GitHub como se explica en [SECURITY.md](SECURITY.md). Por favor no abras un issue público para eso.

## Licencia

Frenchcore Machine es software libre bajo la [GNU Affero General Public License v3.0](LICENSE). Los sonidos que hagas con el plugin son tuyos, sin ninguna restricción.

Hecho con [JUCE](https://juce.com). VST es una marca registrada de Steinberg Media Technologies GmbH.
