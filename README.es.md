# Frenchcore Machine

[English](README.md)

Un sintetizador wavetable en formato plugin con un solo objetivo: hacer kicks de frenchcore rápido.

Frenchcore Machine está a mitad de camino entre arrastrar un sample y construir un kick desde cero en un synth completo. Trae un oscilador wavetable, una envolvente que ya tiene la forma clásica de kick + bass, una capa de click y una cadena de efectos reordenable con lo que un kick de frenchcore realmente necesita. Pensalo como un Serum o un Vital muy simplificado, enfocado en kicks.

> **Estado:** desarrollo inicial. Todavía no hay nada usable. El avance está en el [plan](docs/PLAN.md) (en inglés).

## Funciones (previstas para v0.1.0)

- Oscilador wavetable con wavetables de fábrica para kicks, y carga de wavetables propias (WAV compatible con Serum/Vital).
- Envolvente de amplitud editable (forma kick + bass por defecto) y envolvente de pitch.
- Capa opcional de click/transiente.
- Monofónico con glide opcional; la nota MIDI afina la cola del kick.
- Efectos que podés ordenar arrastrándolos: distorsión (soft clip, hard clip, overdrive, foldback, bitcrush), EQ de 3 bandas y stereo multibanda.
- Presets.
- VST3, AU y Standalone para Windows, macOS y Linux.

La lista completa está en el [MVP scope](docs/MVP_SCOPE.md).

## Descarga

Todavía no hay versiones publicadas. Cuando las haya van a estar en la [página de Releases](https://github.com/nocturn444/frenchcore-machine/releases). Las versiones de desarrollo se adjuntan a cada ejecución en la pestaña Actions.

## Compilar desde el código

Ver [docs/BUILDING.md](docs/BUILDING.md).

## Documentación

Toda la documentación está en [docs/](docs/README.md), en inglés. Para empezar:

- [Plan de desarrollo](docs/PLAN.md)
- [Arquitectura](docs/ARCHITECTURE.md)
- [Glosario](docs/GLOSSARY.md) con los términos técnicos

## Contribuir

Las contribuciones son bienvenidas. Leé [CONTRIBUTING.md](CONTRIBUTING.md) antes: el proyecto sigue desarrollo guiado por documentación, así que cada función empieza con una especificación escrita.

## Licencia

Frenchcore Machine es software libre bajo la [GNU Affero General Public License v3.0](LICENSE). Los sonidos que hagas con el plugin son tuyos, sin ninguna restricción.

Hecho con [JUCE](https://juce.com). VST es una marca registrada de Steinberg Media Technologies GmbH.
