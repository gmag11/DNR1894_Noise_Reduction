## Why

El repositorio no tiene todavía una definición formal para un plugin VST que emule el comportamiento del LM1894, un sistema de reducción dinámica de ruido pensado para material de reproducción con hiss de alta frecuencia. Antes de implementar DSP y UI conviene fijar qué parte del comportamiento del integrado se considera esencial, qué controles deben exponerse al usuario y cómo se validará la fidelidad del resultado.

## What Changes

Se añade una nueva capability para un plugin VST estéreo que modele el comportamiento funcional del LM1894.

La especificación define:
- la arquitectura audible esperada del procesado estéreo con una ruta de control compartida;
- la posibilidad de encadenar varias etapas del mismo proceso para aumentar la pendiente efectiva y la reducción de hiss;
- el uso de JUCE como base de producto para construir un plugin multiplataforma;
- una estrategia de entrega incremental que empieza con VST3 en Windows y deja preparado AU, standalone y VST3 para Linux;
- el rango de ancho de banda dinámico y la respuesta temporal objetivo;
- los parámetros y modos de calibración que el plugin debe ofrecer;
- la instrumentación y las pruebas necesarias para validar la simulación.

## Capabilities

### New Capabilities
- `lm1894-noise-reduction-plugin`: Plugin VST estéreo que simula la reducción dinámica de ruido tipo LM1894 para fuentes de reproducción con ruido de alta frecuencia.

### Modified Capabilities

## Impact

Se verán afectadas las futuras decisiones de:
- arquitectura DSP del plugin;
- framework y sistema de build para targets multiplataforma;
- estructura de módulos y separación entre shell de plugin, parámetros y núcleo DSP;
- compensación de cutoff y ganancia al cambiar el número de etapas en cascada;
- diseño de parámetros y estado persistente;
- UI y metering para visualización de ancho de banda y reducción;
- estrategia de validación con señales sintéticas y material de audio de referencia.