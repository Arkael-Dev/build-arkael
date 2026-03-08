From 3e0cb08ebf72f9d69626742cbc231dcc20d69e97 Mon Sep 17 00:00:00 2001
From: Sean Du <do4suki@gmail.com>
Date: Sat, 8 Mar 2025 02:02:55 +0800
Subject: [PATCH] [SQUASH] drivers: of: Revert multiple "address and unittest"
 changes

The WiFi HAL fails to initialize due to this commit.

Revert "of: address: Preserve the flags portion on 1:1 dma-ranges mapping"

This reverts commit ad9ec26aba9b3a9be23eebe206e9c87621fceeec.

Change-Id: Idf3e861ed77bf628e5bc1c8131098609cf93eeac
Signed-off-by: Sean Du <do4suki@gmail.com>

Revert "of: address: Store number of bus flag cells rather than bool"

This reverts commit 007662f7851519253d357d381bff576d7472a37c.

Change-Id: Ib5e0e9e6718a6f9f9a87008ab1b0a23a0f607b87
Signed-off-by: Sean Du <do4suki@gmail.com>

Revert "of: address: Remove duplicated functions"

This reverts commit 7eb954ec9648a1c4e884621bcebdf3a1d209a92b.

Change-Id: Iea3bc044df98eef3580d00d53b222e61e2e38f6c
Signed-off-by: Sean Du <do4suki@gmail.com>

Revert "of: address: Fix address translation when address-size is greater than 2"

This reverts commit 30eb1123b25d99b0de1a4ea1fd4c4abaf8c9040e.

Change-Id: I1bc5aafef6b5534fcc83fb66f078905b54d08620
Signed-off-by: Sean Du <do4suki@gmail.com>

Revert "of/address: Add support for 3 address cell bus"

This reverts commit 46dfdb0f933a7e12c7b9dbc73e93ce4be0174a0f.

Change-Id: I384bd412338f08bf8224808d6b6474a4e798b22c
Signed-off-by: Sean Du <do4suki@gmail.com>

Revert "of: unittest: Add bus address range parsing tests"

This reverts commit 57e3220c28bd7f637b562f0d7aee6cbc6d3f32c5.

Change-Id: Ie6de0dd08607bb36f91eb9feccc41ac66f2cca2e
Signed-off-by: Sean Du <do4suki@gmail.com>
Signed-off-by: chandu078 <chandudyavanapelli03@gmail.com>
---
 drivers/of/address.c                        |  76 ++++----------
 drivers/of/unittest-data/tests-address.dtsi |   9 +-
 drivers/of/unittest.c                       | 109 --------------------
 3 files changed, 21 insertions(+), 173 deletions(-)

diff --git a/drivers/of/address.c b/drivers/of/address.c
index 5c79e3e2a8ac5..8e51a80e22e7e 100644
--- a/drivers/of/address.c
+++ b/drivers/of/address.c
@@ -50,7 +50,7 @@ struct of_bus {
 	u64		(*map)(__be32 *addr, const __be32 *range,
 				int na, int ns, int pna);
 	int		(*translate)(__be32 *addr, u64 offset, int na);
-	int		flag_cells;
+	bool	has_flags;
 	unsigned int	(*get_flags)(const __be32 *addr);
 };
 
@@ -95,43 +95,11 @@ static int of_bus_default_translate(__be32 *addr, u64 offset, int na)
 	return 0;
 }
 
-static unsigned int of_bus_default_flags_get_flags(const __be32 *addr)
-{
-	return of_read_number(addr, 1);
-}
-
 static unsigned int of_bus_default_get_flags(const __be32 *addr)
 {
 	return IORESOURCE_MEM;
 }
 
-static u64 of_bus_default_flags_map(__be32 *addr, const __be32 *range, int na,
-				    int ns, int pna)
-{
-	u64 cp, s, da;
-
-	/* Check that flags match */
-	if (*addr != *range)
-		return OF_BAD_ADDR;
-
-	/* Read address values, skipping high cell */
-	cp = of_read_number(range + 1, na - 1);
-	s  = of_read_number(range + na + pna, ns);
-	da = of_read_number(addr + 1, na - 1);
-
-	pr_debug("default flags map, cp=%llx, s=%llx, da=%llx\n", cp, s, da);
-
-	if (da < cp || da >= (cp + s))
-		return OF_BAD_ADDR;
-	return da - cp;
-}
-
-static int of_bus_default_flags_translate(__be32 *addr, u64 offset, int na)
-{
-	/* Keep "flags" part (high cell) in translated address */
-	return of_bus_default_translate(addr + 1, offset, na - 1);
-}
-
 #ifdef CONFIG_PCI
 static unsigned int of_bus_pci_get_flags(const __be32 *addr)
 {
@@ -221,6 +189,10 @@ static u64 of_bus_pci_map(__be32 *addr, const __be32 *range, int na, int ns,
 	return da - cp;
 }
 
+static int of_bus_pci_translate(__be32 *addr, u64 offset, int na)
+{
+	return of_bus_default_translate(addr + 1, offset, na - 1);
+}
 #endif /* CONFIG_PCI */
 
 int of_pci_address_to_resource(struct device_node *dev, int bar,
@@ -330,6 +302,11 @@ static u64 of_bus_isa_map(__be32 *addr, const __be32 *range, int na, int ns,
 	return da - cp;
 }
 
+static int of_bus_isa_translate(__be32 *addr, u64 offset, int na)
+{
+	return of_bus_default_translate(addr + 1, offset, na - 1);
+}
+
 static unsigned int of_bus_isa_get_flags(const __be32 *addr)
 {
 	unsigned int flags = 0;
@@ -342,11 +319,6 @@ static unsigned int of_bus_isa_get_flags(const __be32 *addr)
 	return flags;
 }
 
-static int of_bus_default_flags_match(struct device_node *np)
-{
-	return of_bus_n_addr_cells(np) == 3;
-}
-
 /*
  * Array of bus specific translators
  */
@@ -360,8 +332,8 @@ static struct of_bus of_busses[] = {
 		.match = of_bus_pci_match,
 		.count_cells = of_bus_pci_count_cells,
 		.map = of_bus_pci_map,
-		.translate = of_bus_default_flags_translate,
-		.flag_cells = 1,
+		.translate = of_bus_pci_translate,
+		.has_flags = true,
 		.get_flags = of_bus_pci_get_flags,
 	},
 #endif /* CONFIG_PCI */
@@ -372,21 +344,10 @@ static struct of_bus of_busses[] = {
 		.match = of_bus_isa_match,
 		.count_cells = of_bus_isa_count_cells,
 		.map = of_bus_isa_map,
-		.translate = of_bus_default_flags_translate,
-		.flag_cells = 1,
+		.translate = of_bus_isa_translate,
+		.has_flags = true,
 		.get_flags = of_bus_isa_get_flags,
 	},
-	/* Default with flags cell */
-	{
-		.name = "default-flags",
-		.addresses = "reg",
-		.match = of_bus_default_flags_match,
-		.count_cells = of_bus_default_count_cells,
-		.map = of_bus_default_flags_map,
-		.translate = of_bus_default_flags_translate,
-		.flag_cells = 1,
-		.get_flags = of_bus_default_flags_get_flags,
-	},
 	/* Default */
 	{
 		.name = "default",
@@ -466,8 +427,7 @@ static int of_translate_one(struct device_node *parent, struct of_bus *bus,
 	}
 	if (ranges == NULL || rlen == 0) {
 		offset = of_read_number(addr, na);
-		/* set address to zero, pass flags through */
-		memset(addr + pbus->flag_cells, 0, (pna - pbus->flag_cells) * 4);
+		memset(addr, 0, pna * 4);
 		pr_debug("empty ranges; 1:1 translation\n");
 		goto finish;
 	}
@@ -795,7 +755,7 @@ struct of_pci_range *of_pci_range_parser_one(struct of_pci_range_parser *parser,
 	int na = parser->na;
 	int ns = parser->ns;
 	int np = parser->pna + na + ns;
-	int busflag_na = parser->bus->flag_cells;
+	int busflag_na = 0;
 
 	if (!range)
 		return NULL;
@@ -805,6 +765,10 @@ struct of_pci_range *of_pci_range_parser_one(struct of_pci_range_parser *parser,
 
 	range->flags = parser->bus->get_flags(parser->range);
 
+	/* A extra cell for resource flags */
+	if (parser->bus->has_flags)
+		busflag_na = 1;
+
 	range->bus_addr = of_read_number(parser->range + busflag_na, na - busflag_na);
 
 	if (parser->dma)
diff --git a/drivers/of/unittest-data/tests-address.dtsi b/drivers/of/unittest-data/tests-address.dtsi
index bc0029cbf8eaf..6604a52bf6cb8 100644
--- a/drivers/of/unittest-data/tests-address.dtsi
+++ b/drivers/of/unittest-data/tests-address.dtsi
@@ -14,7 +14,7 @@
 			#size-cells = <1>;
 			/* ranges here is to make sure we don't use it for
 			 * dma-ranges translation */
-			ranges = <0x70000000 0x70000000 0x50000000>,
+			ranges = <0x70000000 0x70000000 0x40000000>,
 				 <0x00000000 0xd0000000 0x20000000>;
 			dma-ranges = <0x0 0x20000000 0x40000000>;
 
@@ -43,13 +43,6 @@
 					     <0x42000000 0x0 0xc0000000 0x20000000 0x0 0x10000000>;
 			};
 
-			bus@a0000000 {
-				#address-cells = <3>;
-				#size-cells = <2>;
-				ranges = <0xf00baa 0x0 0x0 0xa0000000 0x0 0x100000>,
-					 <0xf00bee 0x1 0x0 0xb0000000 0x0 0x200000>;
-			};
-
 		};
 	};
 };
diff --git a/drivers/of/unittest.c b/drivers/of/unittest.c
index 598e0891533fd..ce1386074e66b 100644
--- a/drivers/of/unittest.c
+++ b/drivers/of/unittest.c
@@ -1019,113 +1019,6 @@ static void __init of_unittest_pci_dma_ranges(void)
 	of_node_put(np);
 }
 
-static void __init of_unittest_bus_ranges(void)
-{
-	struct device_node *np;
-	struct of_range range;
-	struct of_range_parser parser;
-	int i = 0;
-
-	np = of_find_node_by_path("/testcase-data/address-tests");
-	if (!np) {
-		pr_err("missing testcase data\n");
-		return;
-	}
-
-	if (of_range_parser_init(&parser, np)) {
-		pr_err("missing ranges property\n");
-		return;
-	}
-
-	/*
-	 * Get the "ranges" from the device tree
-	 */
-	for_each_of_range(&parser, &range) {
-		unittest(range.flags == IORESOURCE_MEM,
-			"for_each_of_range wrong flags on node %pOF flags=%x (expected %x)\n",
-			np, range.flags, IORESOURCE_MEM);
-		if (!i) {
-			unittest(range.size == 0x50000000,
-				 "for_each_of_range wrong size on node %pOF size=%llx\n",
-				 np, range.size);
-			unittest(range.cpu_addr == 0x70000000,
-				 "for_each_of_range wrong CPU addr (%llx) on node %pOF",
-				 range.cpu_addr, np);
-			unittest(range.bus_addr == 0x70000000,
-				 "for_each_of_range wrong bus addr (%llx) on node %pOF",
-				 range.pci_addr, np);
-		} else {
-			unittest(range.size == 0x20000000,
-				 "for_each_of_range wrong size on node %pOF size=%llx\n",
-				 np, range.size);
-			unittest(range.cpu_addr == 0xd0000000,
-				 "for_each_of_range wrong CPU addr (%llx) on node %pOF",
-				 range.cpu_addr, np);
-			unittest(range.bus_addr == 0x00000000,
-				 "for_each_of_range wrong bus addr (%llx) on node %pOF",
-				 range.pci_addr, np);
-		}
-		i++;
-	}
-
-	of_node_put(np);
-}
-
-static void __init of_unittest_bus_3cell_ranges(void)
-{
-	struct device_node *np;
-	struct of_range range;
-	struct of_range_parser parser;
-	int i = 0;
-
-	np = of_find_node_by_path("/testcase-data/address-tests/bus@a0000000");
-	if (!np) {
-		pr_err("missing testcase data\n");
-		return;
-	}
-
-	if (of_range_parser_init(&parser, np)) {
-		pr_err("missing ranges property\n");
-		return;
-	}
-
-	/*
-	 * Get the "ranges" from the device tree
-	 */
-	for_each_of_range(&parser, &range) {
-		if (!i) {
-			unittest(range.flags == 0xf00baa,
-				 "for_each_of_range wrong flags on node %pOF flags=%x\n",
-				 np, range.flags);
-			unittest(range.size == 0x100000,
-				 "for_each_of_range wrong size on node %pOF size=%llx\n",
-				 np, range.size);
-			unittest(range.cpu_addr == 0xa0000000,
-				 "for_each_of_range wrong CPU addr (%llx) on node %pOF",
-				 range.cpu_addr, np);
-			unittest(range.bus_addr == 0x0,
-				 "for_each_of_range wrong bus addr (%llx) on node %pOF",
-				 range.pci_addr, np);
-		} else {
-			unittest(range.flags == 0xf00bee,
-				 "for_each_of_range wrong flags on node %pOF flags=%x\n",
-				 np, range.flags);
-			unittest(range.size == 0x200000,
-				 "for_each_of_range wrong size on node %pOF size=%llx\n",
-				 np, range.size);
-			unittest(range.cpu_addr == 0xb0000000,
-				 "for_each_of_range wrong CPU addr (%llx) on node %pOF",
-				 range.cpu_addr, np);
-			unittest(range.bus_addr == 0x100000000,
-				 "for_each_of_range wrong bus addr (%llx) on node %pOF",
-				 range.pci_addr, np);
-		}
-		i++;
-	}
-
-	of_node_put(np);
-}
-
 static void __init of_unittest_parse_interrupts(void)
 {
 	struct device_node *np;
@@ -3628,8 +3521,6 @@ static int __init of_unittest(void)
 	of_unittest_dma_get_max_cpu_address();
 	of_unittest_parse_dma_ranges();
 	of_unittest_pci_dma_ranges();
-	of_unittest_bus_ranges();
-	of_unittest_bus_3cell_ranges();
 	of_unittest_match_node();
 	of_unittest_platform_populate();
 	of_unittest_overlay();