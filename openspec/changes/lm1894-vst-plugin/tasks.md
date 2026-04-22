## 1. Model Definition

- [x] 1.1 Derivar del datasheet y de AN-0390 el rango objetivo de cutoff, el enlace estéreo y la dinámica temporal por defecto.
- [x] 1.2 Definir la traducción entre parámetros internos del modelo y parámetros expuestos al usuario.
- [x] 1.3 Documentar tolerancias aceptables para ataque, release y rango de ancho de banda frente a la referencia analógica.
- [x] 1.4 Definir la ley de compensación de cutoff para 1..N etapas en cascada manteniendo un significado consistente del ancho de banda global.
- [x] 1.5 Fijar y documentar la curva no lineal `detector equivalente -> cutoff global` usando puntos ancla derivados de las figuras clave del datasheet.
- [x] 1.6 Definir la ponderación del sidechain para que la energía en 2 kHz-10 kHz gobierne la apertura mucho más que el contenido subgrave.

## 2. DSP Implementation

- [x] 2.1 Implementar la ruta de control compartida con suma estéreo, ponderación frecuencial, detector y suavizado temporal.
- [x] 2.2 Implementar dos filtros paso bajo enlazados con cutoff continuo y comportamiento estable en tiempo real.
- [x] 2.3 Añadir perfiles de fuente reutilizando la misma arquitectura base del modelo.
- [x] 2.4 Permitir encadenar varias etapas idénticas en serie reutilizando el mismo control global y sin introducir artefactos al cambiar el número de etapas.
- [x] 2.5 Aislar el núcleo DSP en una capa independiente del framework para que pueda probarse sin depender de JUCE ni del host.

## 3. Architecture And State

- [x] 3.1 Definir la estructura de módulos del proyecto separando `plugin/juce`, `src/dsp`, `src/params`, `src/app` y `tests`.
- [x] 3.2 Definir un `ParameterLayout` único con identificadores estables, defaults, rangos y conversiones a unidades internas.
- [x] 3.3 Definir `PluginState` y el formato de serialización de presets y estado del host.
- [x] 3.4 Definir `MeterState` thread-safe para comunicar actividad al editor sin comprometer tiempo real.

## 4. Plugin Productization

- [ ] 4.1 Crear la base del proyecto con JUCE orientada a un plugin multiplataforma.
- [ ] 4.2 Exponer parámetros de sensibilidad, ancho de banda mínimo y máximo, tiempos, perfil, número de etapas, bypass y mix o trim de salida si procede.
- [ ] 4.3 Diseñar metering para visualizar apertura de ancho de banda y/o reducción de ruido estimada.
- [ ] 4.4 Garantizar persistencia de estado, automatización estable y compatibilidad con operación estéreo en hosts VST.
- [ ] 4.5 Configurar Windows + VST3 como primer target oficial y dejar preparada la generación de standalone.
- [ ] 4.6 Preparar la activación posterior de AU en macOS y VST3/standalone en Linux sin reorganizar el código base.

## 5. Verification

- [ ] 5.1 Crear pruebas con ruido, bursts y tonos para validar ataque, release y respuesta espectral dinámica.
- [ ] 5.2 Verificar que un estímulo brillante en un solo canal no desplace la imagen estéreo al compartir la ruta de control.
- [ ] 5.3 Comparar el comportamiento del plugin en varios sample rates y tamaños de bloque para asegurar consistencia.
- [ ] 5.4 Comparar 1, 2 y varias etapas para confirmar aumento de atenuación en altas frecuencias, compensación correcta de cutoff global y ausencia de inestabilidad audible.
- [ ] 5.5 Verificar automatización, persistencia y carga del plugin en hosts representativos soportados por JUCE.
- [ ] 5.6 Validar la migración de targets comprobando que Windows VST3, macOS AU y Linux VST3/standalone comparten el mismo núcleo DSP y layout de parámetros.
- [ ] 5.7 Validar el mapeo `detector -> cutoff` frente a los puntos ancla definidos y comprobar que el preset `tape` se asienta en una apertura nominal cercana a 2 kHz con ruido de referencia.