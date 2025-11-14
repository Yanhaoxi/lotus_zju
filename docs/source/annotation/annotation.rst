Annotation Framework
===================

Lotus provides annotation frameworks for specifying and analyzing program behavior.

ArgPosition
-----------

Argument position tracking and analysis.

**Location**: ``include/Annotation/ArgPosition.h``

**Features**: Function argument indexing and position-based analysis.

**Usage**:
.. code-block:: cpp

   #include <Annotation/ArgPosition.h>
   ArgPosition pos(function, argIndex);
   unsigned index = pos.getPosition();

ModRef Annotations
-----------------

Modification and reference behavior annotations.

**Location**: ``include/Annotation/ModRef/``

**Components**:
* **ExternalModRefTable** – External function mod/ref specifications
* **ExternalModRefTablePrinter** – Annotation output utilities

**Usage**:
.. code-block:: cpp

   #include <Annotation/ExternalModRefTable.h>
   ExternalModRefTable table;
   bool mayRead = table.mayRead(function, pointer);

Pointer Annotations
------------------

Pointer behavior and safety annotations.

**Location**: ``include/Annotation/Pointer/``

**Components**:
* **ExternalPointerTable** – External pointer specifications
* **ExternalPointerTablePrinter** – Pointer annotation output

**Usage**:
.. code-block:: cpp

   #include <Annotation/ExternalPointerTable.h>
   ExternalPointerTable table;
   bool isSafe = table.isDereferenceSafe(pointer);

Taint Analysis Annotations
-------------------------

Information flow and taint propagation specifications.

**Location**: ``include/Annotation/Taint/``

**Components**:
* **TaintConfigManager** – Taint configuration management
* **TaintConfigParser** – Taint specification parsing

**Usage**:
.. code-block:: cpp

   #include <Annotation/Taint/TaintConfigManager.h>
   TaintConfigManager config("taint.spec");
   bool isTainted = config.isTainted(value);
