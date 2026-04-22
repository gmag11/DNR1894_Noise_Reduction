## Context

El LM1894 implementa un sistema DNR no complementario para reproducción estéreo. Su núcleo audible es un par de filtros paso bajo con pendiente de 6 dB/octava cuyo ancho de banda varía aproximadamente entre 1 kHz y 35 kHz. Ambos canales comparten una única ruta de control para evitar desplazamientos de la imagen estéreo. La ruta de control suma L+R, aplica ponderación en frecuencia, detecta picos y gobierna la apertura del filtro con un ataque típico del orden de 500 us y un decay del orden de 60 ms.

La nota de aplicación AN-0390 añade contexto importante para producto: calibración por umbral, ubicación del procesado antes de tono/volumen, perfiles prácticos para cassette, FM y TV, y el hecho de que la reducción subjetiva del hiss depende del espectro y del nivel de ruido de la fuente.

## Goals / Non-Goals

**Goals:**
- Modelar el comportamiento audible principal del LM1894 en un plugin VST estéreo de tiempo real.
- Preparar el producto para distribución multiplataforma sobre Windows y macOS sin duplicar la implementación del DSP.
- Empezar con una entrega mínima en Windows VST3 sin cerrar el camino a AU, standalone o Linux VST3/standalone.
- Mantener una única ruta de control enlazada para preservar la imagen estéreo.
- Permitir una arquitectura opcional multietapa para obtener mayor reducción y pendientes más pronunciadas.
- Exponer controles que permitan calibrar sensibilidad, rango de ancho de banda y dinámica temporal.
- Permitir validación objetiva frente a objetivos extraídos del datasheet y la nota de aplicación.

**Non-Goals:**
- Emular el silicio a nivel transistor o reproducir exactamente tensiones internas de cada pin.
- Simular el resto de la cadena analógica externa completa de cassette, FM o TV.
- Reproducir intencionadamente limitaciones de alimentación, PSRR o distorsión no relevantes para el resultado audible principal.
- Soportar desde la primera iteración todos los formatos de plugin posibles; la prioridad es VST3 y, si conviene al producto, AU y standalone usando la misma base.
- Introducir dependencias específicas de plataforma dentro del núcleo DSP o del modelo de parámetros.

## Decisions

### 1. Modelado funcional, no eléctrico

La primera versión se especifica como una simulación funcional del comportamiento audible del LM1894, no como una emulación circuito a circuito. Esto mantiene el alcance controlado y alinea la implementación con lo que realmente importa en un plugin: la respuesta espectral dinámica, la preservación de imagen estéreo y la sensación de reducción de hiss.

### 2. JUCE como framework base de producto

JUCE es una elección técnicamente correcta para este proyecto porque separa bien las preocupaciones de producto y de DSP: permite construir VST3, AU y aplicación standalone con una misma base de código, resuelve gran parte de la integración con hosts y automatización, y deja el modelo LM1894 encapsulado en una capa DSP propia.

La decisión no implica acoplar el algoritmo al framework. El núcleo DSP debe quedar aislado en una capa portable y testeable, con JUCE encargado de:
- ciclo de vida del plugin;
- parámetros, presets y automatización;
- editor y metering;
- targets y build multiplataforma.

```text
    +---------------------------+
    |        JUCE Shell         |
    | VST3 / AU / Standalone    |
    +------------+--------------+
                     |
        parameters|audio buffers
                     v
    +---------------------------+
    |     LM1894 DSP Core       |
    | sidechain + filters + N   |
    | cascaded stages           |
    +---------------------------+
```

Esto reduce riesgo de reescritura posterior si el plugin termina necesitando más de un formato de entrega.

### 3. Estrategia de targets por fases

La estrategia recomendada es:

1. V1 inicial: Windows + VST3.
2. V1.x: añadir standalone sobre la misma base JUCE para depuración y validación fuera del host.
3. V2: añadir macOS + AU.
4. V2.x: añadir Linux con VST3 y standalone si la validación en host lo justifica.

Esto mantiene el arranque pequeño sin tomar decisiones que luego obliguen a rehacer la estructura. AU sólo tiene sentido en macOS, así que la ruta natural para Linux debe ser VST3 y standalone, no AU.

La clave no es activar todos los targets desde el primer día, sino que el proyecto quede organizado para que cada nuevo formato sea una integración adicional, no una reescritura.

### 4. Arquitectura del proyecto

La arquitectura propuesta separa producto, parámetros y DSP en capas explícitas:

```text
repo/
    plugin/
        juce/
            CMakeLists.txt
            Source/
                PluginProcessor.*
                PluginEditor.*
                StandaloneMain.*
    src/
        dsp/
            Lm1894Model.h
            Lm1894Model.cpp
            SidechainDetector.h
            SidechainDetector.cpp
            VariableLpStage.h
            VariableLpStage.cpp
            CascadedFilterBank.h
            CascadedFilterBank.cpp
        params/
            ParameterIds.h
            ParameterLayout.h
            ParameterLayout.cpp
            PresetProfiles.h
            PresetProfiles.cpp
        app/
            PluginState.h
            PluginState.cpp
            MeterState.h
            MeterState.cpp
    tests/
        dsp/
        integration/
    docs/
```

Responsabilidades por capa:
- `src/dsp`: algoritmo puro, sin clases UI ni tipos dependientes del host.
- `src/params`: definición única de parámetros, rangos, escalados, defaults y perfiles.
- `src/app`: estado compartido entre shell de plugin, DSP y metering.
- `plugin/juce`: pegamento con JUCE, formatos de plugin, editor y lifecycle del host.
- `tests`: validación del modelo y pruebas de integración del plugin.

### 5. Flujo de audio y control

```text
Host/DAW
     |
     v
JUCE AudioProcessor
     |
     +--> Parameter snapshot -----------------------------+
     |                                                    |
     v                                                    v
Input buffer --> LM1894 Model --> Output buffer --> MeterState --> Editor
                                    |
                                    +--> Stereo sum -> weighting -> detector -> cutoff target
                                                                            |
                                                                            +--> stage compensation -> N cascaded stages
```

La regla importante es que el `AudioProcessor` no contenga el algoritmo en línea. Debe actuar como adaptador entre buffers del host, snapshots atómicos de parámetros y el núcleo `Lm1894Model`.

### 6. Modelo de parámetros

Los parámetros deben definirse una sola vez en `src/params` y consumirse desde JUCE sin duplicar semántica. El conjunto mínimo queda así:
- `Sensitivity`
- `MinBandwidthHz`
- `MaxBandwidthHz`
- `AttackMs`
- `ReleaseMs`
- `StageCount`
- `SourceProfile`
- `Bypass`
- `OutputTrimDb`
- `Mix` si finalmente se considera útil

Cada parámetro debe tener:
- identificador estable;
- rango normalizado para automatización;
- conversión a unidades físicas internas;
- valor por defecto;
- texto de display desacoplado del DSP.

### 7. Estado y presets

`PluginState` debe encapsular:
- snapshot de parámetros activos;
- serialización de estado para el host;
- perfil de fuente seleccionado;
- versión interna de preset o schema de estado.

Esto evita que el serializado dependa de detalles del editor o de objetos concretos de JUCE difíciles de migrar.

### 8. Metros y editor

El editor JUCE debe leer un `MeterState` thread-safe alimentado por el hilo de audio con datos mínimos agregados, por ejemplo:
- apertura de ancho de banda actual;
- actividad del detector;
- número de etapas activas;
- estado de bypass.

No debe exponer referencias directas al estado interno del filtro ni forzar locks en tiempo real.

### 9. Build y portabilidad

El build debería apoyarse en CMake con JUCE para evitar depender del Projucer como fuente única de verdad. La configuración inicial recomendada es:
- target `Lm1894Vst3` en Windows;
- opción de compilar `Standalone` en el mismo proyecto;
- flags o presets para habilitar macOS AU y Linux VST3/standalone más adelante.

Esto te deja una ruta de migración bastante limpia:
- el DSP y parámetros no cambian;
- cambia sólo el packaging de salida y las tareas de CI/firmado por plataforma.

### 10. Arquitectura interna enlazada en estéreo

El plugin usará dos rutas de audio equivalentes y una sola ruta de control compartida.

```text
        Left In --------------------> Variable LPF -----------------> Left Out
            |                               ^
            |                               |
            +----\                     control signal
                  > Sum -> Weight -> Peak/Envelope -> Bandwidth map
            +----/                     control signal
            |
        Right In -------------------> Variable LPF -----------------> Right Out
```

Esta decisión refleja el comportamiento del integrado y evita que un evento brillante en un solo canal estreche o abra sólo un lado del estéreo.

### 11. Un filtro principal simple y continuamente variable por etapa

La pendiente objetivo base en cada etapa será equivalente a un paso bajo de primer orden por canal. La simulación debe priorizar continuidad temporal del cutoff y ausencia de zipper noise sobre exactitud topológica de la implementación analógica. La forma concreta del filtro digital queda abierta siempre que cumpla los requisitos de respuesta y estabilidad definidos en la spec.

### 12. Modo multietapa con sidechain global compartido

El plugin podrá encadenar varias etapas idénticas del mismo proceso en serie para aumentar la pendiente efectiva del filtrado y la reducción de hiss. Todas las etapas deberán compartir el mismo estado de control para conservar un comportamiento coherente y evitar aperturas divergentes entre etapas.

```text
        Left In ---> Stage 1 LPF ---> Stage 2 LPF ---> ... ---> Stage N LPF ---> Left Out
            |             ^               ^                           ^
            |             |               |                           |
            +------\      |               |                           |
                   > Sum -> Weight -> Peak/Envelope -> Bandwidth map --+
            +------/      |               |                           |
            |             |               |                           |
        Right In --> Stage 1 LPF ---> Stage 2 LPF ---> ... ---> Stage N LPF --> Right Out
```

La referencia analógica de AN-0390 documenta explícitamente el caso de dos etapas equivalentes para lograr una pendiente de 12 dB/oct y hasta unos 18 dB de reducción. En el plugin, la arquitectura deberá generalizar esa idea a varias etapas sin cambiar el principio del proceso.

### 13. Compensación del cutoff al variar el número de etapas

Al cascadar etapas, el punto de -3 dB combinado cae por debajo del cutoff individual de cada filtro. Para que el control de ancho de banda siga significando aproximadamente lo mismo al pasar de 1 a N etapas, el diseño debe compensar el cutoff por etapa a partir del objetivo global.

Para un banco de N etapas de primer orden idénticas, la relación objetivo es:

$$f_{c,global} = f_{c,stage} \sqrt{10^{0.3/N} - 1}$$

Por tanto, el diseño interno deberá calcular un cutoff por etapa a partir del cutoff global deseado, en lugar de reutilizar sin más el mismo cutoff por etapa y aceptar un estrechamiento accidental del rango útil.

### 14. Parámetros físicos internos separados de controles de usuario

La implementación puede mantener parámetros internos expresados como sensibilidad, frecuencia mínima, frecuencia máxima, ataque y release, mientras que la UI podrá agruparlos en controles más musicales o más cercanos al datasheet. Esto permite una experiencia de usuario razonable sin perder trazabilidad con el comportamiento del LM1894.

El número de etapas formará parte de esos parámetros expuestos o de un modo avanzado claramente visible, ya que modifica de forma audible la pendiente y la reducción alcanzable.

### 15. Perfiles de fuente como presets controlados

La spec incluirá perfiles orientados a fuentes reales porque AN-0390 demuestra que la calibración depende del ruido de origen. En vez de obligar a recrear toda la electrónica de cassette, FM o TV, esos perfiles actuarán como presets reproducibles sobre la ruta de control: sensibilidad, ponderación y, cuando aplique, muesca en sidechain para 19 kHz o 15.734 kHz.

### 16. Estrategia multiplataforma

La primera planificación de producto debe asumir:
- Windows como target principal de desarrollo inicial;
- macOS como segundo target soportado por la misma base JUCE;
- formato VST3 como mínimo de entrega;
- AU como salida específica de macOS una vez estabilizada la base;
- Linux orientado a VST3 y standalone, no a AU.

El punto importante es no mezclar lógica de UI o clases de JUCE dentro del núcleo DSP. Si el DSP permanece agnóstico al framework, validar comportamiento y mantener el proyecto será bastante más simple.

### 17. Validación basada en comportamiento

La aceptación no se basará sólo en escucha subjetiva. Se exigirán pruebas con bursts, ruido y barridos para comprobar que el tiempo de apertura/cierre, el enlace estéreo y el rango de cutoff se comportan dentro de tolerancias razonables respecto al LM1894 documentado.

### 18. Plan técnico para una V1 ampliable

El trabajo posterior debería seguir este desglose:

1. Definir una variable global de cutoff objetivo a partir del sidechain compartido.
2. Derivar el cutoff por etapa con compensación por número de etapas.
3. Aplicar el mismo cutoff compensado a N etapas idénticas en ambos canales.
4. Encapsular el DSP en una capa independiente de JUCE con API clara para proceso, reset y actualización de parámetros.
5. Definir `ParameterLayout` y `PluginState` como contrato estable entre host, DSP y editor.
6. Montar un plugin JUCE con VST3 para Windows como target mínimo y dejar preparada la salida standalone.
7. Exponer `Stage Count` y validar que el cambio entre valores mantiene estabilidad, nivel razonable y ausencia de clicks.
8. Añadir pruebas comparativas entre 1, 2 y N etapas para cuantificar la reducción extra y el posible oscurecimiento del material.
9. Cuando la base esté estable, activar AU en macOS y VST3/standalone en Linux sin tocar el núcleo DSP.

## Risks / Trade-offs

- La relación exacta entre señal, ruido y cutoff del LM1894 es no lineal; una simulación digital demasiado simplificada puede sonar bien pero alejarse de la dinámica real del chip.
- Más etapas aumentan la reducción, pero también elevan el riesgo de apagar demasiado el contenido brillante o de producir una sensación menos natural en material rico en agudos.
- Si cada etapa tuviera control independiente, el comportamiento sería más complejo pero menos fiel al principio de control común; por eso se prefiere un sidechain global.
- JUCE acelera mucho el producto multiplataforma, pero añade una capa de framework que conviene aislar del DSP para no dificultar pruebas ni portabilidad futura.
- Si el layout de parámetros o la serialización se mezclan demasiado con JUCE desde el inicio, la migración entre formatos será más frágil de lo necesario.
- Los perfiles FM y TV pertenecen más al contexto de aplicación que al núcleo del IC; incluirlos mejora utilidad práctica, pero introduce alcance adicional.
- Si la automatización o el cambio de cutoff no se suavizan correctamente, el plugin puede introducir artefactos que el hardware original no produce de manera perceptible.
- La reducción subjetiva de ruido depende del material; por eso la spec debe definir métricas objetivas y no sólo promesas auditivas amplias.